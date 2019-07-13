#define _POSIX_C_SOURCE 1
#include <assert.h>
#include <errno.h>
#include <mrsh/array.h>
#include <mrsh/shell.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/process.h"
#include "shell/task.h"

static const int ignored_signals[] = {
	SIGINT,
	SIGQUIT,
	SIGTSTP,
	SIGTTIN,
	SIGTTOU,
};

static const size_t IGNORED_SIGNALS_LEN =
	sizeof(ignored_signals) / sizeof(ignored_signals[0]);

bool mrsh_set_job_control(struct mrsh_state *state, bool enabled) {
	assert(state->fd >= 0);

	if (state->job_control == enabled) {
		return true;
	}

	if (enabled) {
		// Loop until we are in the foreground
		while (true) {
			pid_t pgid = getpgrp();
			if (tcgetpgrp(state->fd) == pgid) {
				break;
			}
			kill(-pgid, SIGTTIN);
		}

		// Ignore interactive and job-control signals
		struct sigaction sa = { .sa_handler = SIG_IGN };
		sigemptyset(&sa.sa_mask);
		for (size_t i = 0; i < IGNORED_SIGNALS_LEN; ++i) {
			if (sigaction(ignored_signals[i], &sa, NULL) != 0) {
				fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
				return false;
			}
		}

		// Put ourselves in our own process group
		state->pgid = getpid();
		if (setpgid(state->pgid, state->pgid) != 0) {
			fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
			return false;
		}

		// Grab control of the terminal
		if (tcsetpgrp(state->fd, state->pgid) != 0) {
			fprintf(stderr, "tcsetpgrp failed: %s\n", strerror(errno));
			return false;
		}
		// Save default terminal attributes for the shell
		if (tcgetattr(state->fd, &state->term_modes) != 0) {
			fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
			return false;
		}
	} else {
		return false; // TODO
	}

	state->job_control = enabled;
	return true;
}

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
		(array->len - i - 1) * sizeof(void *));
	--array->len;
}

struct mrsh_job *job_create(struct mrsh_state *state, pid_t pgid) {
	int id = 1;
	for (size_t i = 0; i < state->jobs.len; ++i) {
		struct mrsh_job *job = state->jobs.data[i];
		if (id < job->job_id) {
			id = job->job_id + 1;
		}
	}

	struct mrsh_job *job = calloc(1, sizeof(struct mrsh_job));
	job->state = state;
	job->pgid = pgid;
	job->job_id = id;
	mrsh_array_add(&state->jobs, job);
	return job;
}

void job_destroy(struct mrsh_job *job) {
	if (job == NULL) {
		return;
	}

	if (job->state->foreground_job == job) {
		job_set_foreground(job, false, false);
	}

	struct mrsh_state *state = job->state;
	for (size_t i = 0; i < state->jobs.len; ++i) {
		if (state->jobs.data[i] == job) {
			array_remove(&state->jobs, i);
			break;
		}
	}

	for (size_t j = 0; j < job->processes.len; ++j) {
		process_destroy(job->processes.data[j]);
	}
	mrsh_array_finish(&job->processes);
	free(job);
}

void job_add_process(struct mrsh_job *job, struct process *proc) {
	mrsh_array_add(&job->processes, proc);
}

bool job_set_foreground(struct mrsh_job *job, bool foreground, bool cont) {
	struct mrsh_state *state = job->state;

	// Don't try to continue the job if it's not stopped
	if (job_poll(job) != TASK_STATUS_STOPPED) {
		cont = false;
	}

	if (foreground && state->foreground_job != job) {
		assert(state->foreground_job == NULL);
		// Put the job in the foreground
		tcsetpgrp(state->fd, job->pgid);
		if (cont) {
			// Restore the job's terminal modes
			tcsetattr(state->fd, TCSADRAIN, &job->term_modes);
		}
		state->foreground_job = job;
	}

	if (!foreground && state->foreground_job == job) {
		// Put the shell back in the foreground
		tcsetpgrp(state->fd, state->pgid);
		// Save the job's terminal modes, to restore them if it's put in the
		// foreground again
		tcgetattr(state->fd, &job->term_modes);
		// Restore the shell’s terminal modes
		tcsetattr(state->fd, TCSADRAIN, &state->term_modes);
		state->foreground_job = NULL;
	}

	if (cont) {
		if (kill(-job->pgid, SIGCONT) != 0) {
			fprintf(stderr, "kill failed: %s\n", strerror(errno));
			return false;
		}

		for (size_t j = 0; j < job->processes.len; ++j) {
			struct process *proc = job->processes.data[j];
			proc->stopped = false;
		}
	}

	return true;
}

int job_poll(struct mrsh_job *job) {
	int proc_status = 0;
	bool stopped = false;
	for (size_t j = 0; j < job->processes.len; ++j) {
		struct process *proc = job->processes.data[j];
		proc_status = process_poll(proc);
		if (proc_status == TASK_STATUS_WAIT) {
			return TASK_STATUS_WAIT;
		}
		if (proc_status == TASK_STATUS_STOPPED) {
			stopped = true;
		}
	}

	if (stopped) {
		return TASK_STATUS_STOPPED;
	}
	// All processes have terminated, return the last one's status
	return proc_status;
}

static bool _job_wait(struct mrsh_state *state, pid_t pid) {
	while (true) {
		// We only want to be notified about stopped processes in the main
		// shell. Child processes want to block until their own children have
		// terminated.
		//
		// Here it's important to wait for a specific process: we don't want to
		// steal one of our grandchildren's status for one of our children.
		int stat;
		pid_t ret = waitpid(pid, &stat, state->child ? 0 : WUNTRACED);
		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
			fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
			return false;
		}
		assert(ret > 0);

		update_job(state, ret, stat);
		return true;
	}
}

int job_wait(struct mrsh_job *job) {
	while (true) {
		int status = job_poll(job);
		if (status != TASK_STATUS_WAIT) {
			return status;
		}

		struct process *wait_proc = NULL;
		for (size_t j = 0; j < job->processes.len; ++j) {
			struct process *proc = job->processes.data[j];
			if (process_poll(proc) == TASK_STATUS_WAIT) {
				wait_proc = proc;
				break;
			}
		}
		assert(wait_proc != NULL);
		if (!_job_wait(job->state, wait_proc->pid)) {
			return TASK_STATUS_ERROR;
		}
	}
}

int job_wait_process(struct process *proc) {
	while (true) {
		int status = process_poll(proc);
		if (status != TASK_STATUS_WAIT) {
			return status;
		}

		if (!_job_wait(proc->state, proc->pid)) {
			return TASK_STATUS_ERROR;
		}
	}
}

bool init_job_child_process(struct mrsh_state *state) {
	if (!state->job_control) {
		return true;
	}

	struct sigaction sa = { .sa_handler = SIG_DFL };
	sigemptyset(&sa.sa_mask);
	for (size_t i = 0; i < IGNORED_SIGNALS_LEN; ++i) {
		if (sigaction(ignored_signals[i], &sa, NULL) != 0) {
			fprintf(stderr, "sigaction failed: %s\n", strerror(errno));
			return false;
		}
	}

	return true;
}

void update_job(struct mrsh_state *state, pid_t pid, int stat) {
	update_process(state, pid, stat);

	// Put stopped and terminated jobs in the background. We don't want to do so
	// if we're not the main shell, because we only have a partial view of the
	// jobs (we only know about our own child processes).
	if (!state->child) {
		for (size_t i = 0; i < state->jobs.len; ++i) {
			struct mrsh_job *job = state->jobs.data[i];
			if (job_poll(job) != TASK_STATUS_WAIT) {
				job_set_foreground(job, false, false);
			}
		}
	}
}

struct mrsh_job *job_by_id(struct mrsh_state *state, const char *id) {
	if (id[0] != '%' || id[1] == '\0') {
		fprintf(stderr, "Invalid job ID specifier\n");
		return NULL;
	}

	if (id[2] == '\0') {
		switch (id[1]) {
		case '%':
		case '+':
			for (ssize_t i = state->jobs.len - 1; i >= 0; --i) {
				struct mrsh_job *job = state->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_STOPPED) {
					return job;
				}
			}
			if (state->jobs.len < 1) {
				fprintf(stderr, "No current job\n");
				return NULL;
			}
			return state->jobs.data[state->jobs.len - 1];
		case '-':
			for (ssize_t i = state->jobs.len - 1, n = 0; i >= 0; --i) {
				struct mrsh_job *job = state->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_STOPPED) {
					if (++n == 2) {
						return job;
					}
				}
			}
			if (state->jobs.len < 2) {
				fprintf(stderr, "No previous job\n");
				return NULL;
			}
			return state->jobs.data[state->jobs.len - 2];
		}
	}

	if (id[1] >= '0' && id[1] <= '9') {
		char *endptr;
		int n = strtol(&id[1], &endptr, 10);
		if (endptr[0] != '\0') {
			fprintf(stderr, "Invalid job number '%s'\n", id);
			return NULL;
		}
		for (size_t i = 0; i < state->jobs.len; ++i) {
			struct mrsh_job *job = state->jobs.data[i];
			if (job->job_id == n) {
				return job;
			}
		}
		fprintf(stderr, "No such job '%s' (%d)\n", id, n);
		return NULL;
	}

	if (id[1] == '?') {
		// TODO
		fprintf(stderr, "Job lookup by command string is unimplemented\n");
		return NULL;
	}

	fprintf(stderr, "Job lookup by command string is unimplemented\n");
	return NULL;
}
