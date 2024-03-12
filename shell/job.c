#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/array.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/process.h"
#include "shell/shell.h"
#include "shell/task.h"

bool mrsh_set_job_control(struct mrsh_state *state, bool enabled) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	assert(priv->term_fd >= 0);

	if (priv->job_control == enabled) {
		return true;
	}

	if (enabled) {
		// Loop until we are in the foreground
		while (true) {
			pid_t pgid = getpgrp();
			if (tcgetpgrp(priv->term_fd) == pgid) {
				break;
			}
			kill(-pgid, SIGTTIN);
		}

		// Ignore interactive and job-control signals
		set_job_control_traps(state, true);

		// Put ourselves in our own process group, if we aren't the session
		// leader
		priv->pgid = getpid();
		if (getsid(0) != priv->pgid) {
			if (setpgid(priv->pgid, priv->pgid) != 0) {
				perror("setpgid");
				return false;
			}
		}

		// Grab control of the terminal
		if (tcsetpgrp(priv->term_fd, priv->pgid) != 0) {
			perror("tcsetpgrp");
			return false;
		}
		// Save default terminal attributes for the shell
		if (tcgetattr(priv->term_fd, &priv->term_modes) != 0) {
			perror("tcgetattr");
			return false;
		}
	} else {
		set_job_control_traps(state, false);
	}

	priv->job_control = enabled;
	return true;
}

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
		(array->len - i - 1) * sizeof(void *));
	--array->len;
}

struct mrsh_job *job_create(struct mrsh_state *state,
		const struct mrsh_node *node) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	int id = 1;
	for (size_t i = 0; i < priv->jobs.len; ++i) {
		struct mrsh_job *job = priv->jobs.data[i];
		if (id < job->job_id + 1) {
			id = job->job_id + 1;
		}
	}

	struct mrsh_job *job = calloc(1, sizeof(struct mrsh_job));
	job->state = state;
	job->node = mrsh_node_copy(node);
	job->pgid = -1;
	job->job_id = id;
	job->last_status = TASK_STATUS_WAIT;
	mrsh_array_add(&priv->jobs, job);
	return job;
}

void job_destroy(struct mrsh_job *job) {
	if (job == NULL) {
		return;
	}

	struct mrsh_state_priv *priv = state_get_priv(job->state);

	if (priv->foreground_job == job) {
		job_set_foreground(job, false, false);
	}

	for (size_t i = 0; i < priv->jobs.len; ++i) {
		if (priv->jobs.data[i] == job) {
			array_remove(&priv->jobs, i);
			break;
		}
	}

	for (size_t j = 0; j < job->processes.len; ++j) {
		process_destroy(job->processes.data[j]);
	}
	mrsh_array_finish(&job->processes);
	mrsh_node_destroy(job->node);
	free(job);
}

void job_add_process(struct mrsh_job *job, struct mrsh_process *proc) {
	if (job->pgid <= 0) {
		job->pgid = proc->pid;
	}
	// This can fail because we do it both in the parent and the child
	if (setpgid(proc->pid, job->pgid) != 0 && errno != EPERM) {
		perror("setpgid");
		return;
	}
	mrsh_array_add(&job->processes, proc);
}

static void job_queue_notification(struct mrsh_job *job) {
	struct mrsh_state_priv *priv = state_get_priv(job->state);

	int status = job_poll(job);
	if (status != job->last_status && job->pgid > 0 &&
			priv->foreground_job != job) {
		job->pending_notification = true;
	}
	job->last_status = status;
}

bool job_set_foreground(struct mrsh_job *job, bool foreground, bool cont) {
	struct mrsh_state *state = job->state;
	struct mrsh_state_priv *priv = state_get_priv(state);

	assert(job->pgid > 0);

	if (!priv->job_control) {
		return false;
	}

	// Don't try to continue the job if it's not stopped
	if (job_poll(job) != TASK_STATUS_STOPPED) {
		cont = false;
	}

	if (foreground && priv->foreground_job != job) {
		assert(priv->foreground_job == NULL);
		// Put the job in the foreground
		tcsetpgrp(priv->term_fd, job->pgid);
		if (cont) {
			// Restore the job's terminal modes
			tcsetattr(priv->term_fd, TCSADRAIN, &job->term_modes);
		}
		priv->foreground_job = job;
	}

	if (!foreground && priv->foreground_job == job) {
		// Put the shell back in the foreground
		tcsetpgrp(priv->term_fd, priv->pgid);
		// Save the job's terminal modes, to restore them if it's put in the
		// foreground again
		tcgetattr(priv->term_fd, &job->term_modes);
		// Restore the shell’s terminal modes
		tcsetattr(priv->term_fd, TCSADRAIN, &priv->term_modes);
		priv->foreground_job = NULL;
	}

	if (cont) {
		if (kill(-job->pgid, SIGCONT) != 0) {
			perror("kill");
			return false;
		}

		for (size_t j = 0; j < job->processes.len; ++j) {
			struct mrsh_process *proc = job->processes.data[j];
			proc->stopped = false;
		}
	}

	job_queue_notification(job);

	return true;
}

int job_poll(struct mrsh_job *job) {
	int proc_status = 0;
	bool stopped = false;
	for (size_t j = 0; j < job->processes.len; ++j) {
		struct mrsh_process *proc = job->processes.data[j];
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

static void update_job(struct mrsh_state *state, pid_t pid, int stat);

static bool _job_wait(struct mrsh_state *state, pid_t pid, int options) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	assert(pid > 0 && pid != getpid());

	// We only want to be notified about stopped processes in the main
	// shell. Child processes want to block until their own children have
	// terminated.
	if (!priv->child) {
#ifdef WCONTINUED
		options |= WUNTRACED | WCONTINUED;
#else
		options |= WUNTRACED;
#endif
	}

	while (true) {
		// Here it's important to wait for a specific process: we don't want to
		// steal one of our grandchildren's status for one of our children.
		int stat;
		pid_t ret = waitpid(pid, &stat, options);
		if (ret == 0) { // no status available
			assert(options & WNOHANG);
			return true;
		} else if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}
			fprintf(stderr, "waitpid(%d): %s\n", pid, strerror(errno));
			return false;
		}
		assert(ret == pid);

		update_job(state, ret, stat);
		return true;
	}
}

static struct mrsh_process *job_get_running_process(struct mrsh_job *job) {
	for (size_t j = 0; j < job->processes.len; ++j) {
		struct mrsh_process *proc = job->processes.data[j];
		if (process_poll(proc) == TASK_STATUS_WAIT) {
			return proc;
		}
	}
	return NULL;
}

int job_wait(struct mrsh_job *job) {
	while (true) {
		int status = job_poll(job);
		if (status != TASK_STATUS_WAIT) {
			return status;
		}

		struct mrsh_process *wait_proc = job_get_running_process(job);
		assert(wait_proc != NULL);
		if (!_job_wait(job->state, wait_proc->pid, 0)) {
			return TASK_STATUS_ERROR;
		}
	}
}

int job_wait_process(struct mrsh_process *proc) {
	while (true) {
		int status = process_poll(proc);
		if (status != TASK_STATUS_WAIT) {
			return status;
		}

		if (!_job_wait(proc->state, proc->pid, 0)) {
			return TASK_STATUS_ERROR;
		}
	}
}

bool refresh_jobs_status(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	for (size_t i = 0; i < priv->jobs.len; ++i) {
		struct mrsh_job *job = priv->jobs.data[i];
		for (size_t j = 0; j < job->processes.len; ++j) {
			struct mrsh_process *proc = job->processes.data[j];
			int status = process_poll(proc);
			if (status == TASK_STATUS_WAIT || status == TASK_STATUS_STOPPED) {
				if (!_job_wait(job->state, proc->pid, WNOHANG)) {
					return false;
				}
			}
		}
	}

	return true;
}

bool init_job_child_process(struct mrsh_state *state) {
	return mrsh_set_job_control(state, false);
}

static void update_job(struct mrsh_state *state, pid_t pid, int stat) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	update_process(state, pid, stat);

	if (!priv->job_control) {
		return;
	}

	// Put stopped and terminated jobs in the background. We don't want to do so
	// if we're not the main shell, because we only have a partial view of the
	// jobs (we only know about our own child processes).
	for (size_t i = 0; i < priv->jobs.len; ++i) {
		struct mrsh_job *job = priv->jobs.data[i];

		int status = job_poll(job);
		job_queue_notification(job);
		if (status != TASK_STATUS_WAIT && job->pgid > 0) {
			job_set_foreground(job, false, false);
		}
	}
}

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_204
struct mrsh_job *job_by_id(struct mrsh_state *state,
		const char *id, bool interactive) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	if (id[0] != '%' || id[1] == '\0') {
		if (interactive) {
			fprintf(stderr, "Invalid job ID specifier\n");
		}
		return NULL;
	}

	if (id[2] == '\0') {
		switch (id[1]) {
		case '%':
		case '+':
			// Current job
			for (ssize_t i = priv->jobs.len - 1; i >= 0; --i) {
				struct mrsh_job *job = priv->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_STOPPED) {
					return job;
				}
			}
			for (ssize_t i = priv->jobs.len - 1; i >= 0; --i) {
				struct mrsh_job *job = priv->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_WAIT) {
					return job;
				}
			}
			if (interactive) {
				fprintf(stderr, "No current job\n");
			}
			return NULL;
		case '-':
			// Previous job
			for (ssize_t i = priv->jobs.len - 1, n = 0; i >= 0; --i) {
				struct mrsh_job *job = priv->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_STOPPED) {
					if (++n == 2) {
						return job;
					}
				}
			}
			bool first = true;
			for (ssize_t i = priv->jobs.len - 1; i >= 0; --i) {
				struct mrsh_job *job = priv->jobs.data[i];
				if (job_poll(job) == TASK_STATUS_WAIT) {
					if (first) {
						first = false;
						continue;
					}
					return job;
				}
			}
			if (interactive) {
				fprintf(stderr, "No previous job\n");
			}
			return NULL;
		}
	}

	if (id[1] >= '0' && id[1] <= '9') {
		char *endptr;
		int n = strtol(&id[1], &endptr, 10);
		if (endptr[0] != '\0') {
			if (interactive) {
				fprintf(stderr, "Invalid job number '%s'\n", id);
			}
			return NULL;
		}
		for (size_t i = 0; i < priv->jobs.len; ++i) {
			struct mrsh_job *job = priv->jobs.data[i];
			if (job->job_id == n) {
				return job;
			}
		}
		if (interactive) {
			fprintf(stderr, "No such job '%s' (%d)\n", id, n);
		}
		return NULL;
	}

	for (size_t i = 0; i < priv->jobs.len; i++) {
		struct mrsh_job *job = priv->jobs.data[i];
		char *cmd = mrsh_node_format(job->node);
		bool match = false;
		switch (id[1]) {
		case '?':
			match = strstr(cmd, &id[2]) != NULL;
			break;
		default:
			match = strstr(cmd, &id[1]) == cmd;
			break;
		}
		free(cmd);
		if (match) {
			return job;
		}
	}

	if (interactive) {
		fprintf(stderr, "No such job '%s'\n", id);
	}
	return NULL;
}

const char *job_state_str(struct mrsh_job *job, bool r) {
	int status = job_poll(job);
	switch (status) {
	case TASK_STATUS_WAIT:
		return "Running";
	case TASK_STATUS_ERROR:
		return "Error";
	case TASK_STATUS_STOPPED:
		if (job->processes.len > 0) {
			struct mrsh_process *proc = job->processes.data[0];
			switch (proc->signal) {
			case SIGSTOP:
				return r ? "Stopped (SIGSTOP)" : "Suspended (SIGSTOP)";
			case SIGTTIN:
				return r ? "Stopped (SIGTTIN)" : "Suspended (SIGTTIN)";
			case SIGTTOU:
				return r ? "Stopped (SIGTTOU)" : "Suspended (SIGTTOU)";
			}
		}
		return r ? "Stopped" : "Suspended";
	default:
		if (job->processes.len > 0) {
			struct mrsh_process *proc = job->processes.data[0];
			if (proc->stat != 0) {
				static char stat[128];
				snprintf(stat, sizeof(stat), "Done(%d)", proc->stat);
				return stat;
			}
		}
		assert(status >= 0);
		return "Done";
	}
}

void broadcast_sighup_to_jobs(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);
	assert(priv->job_control);

	for (size_t i = 0; i < priv->jobs.len; ++i) {
		struct mrsh_job *job = priv->jobs.data[i];
		if (job_poll(job) >= 0) {
			continue;
		}
		if (kill(-job->pgid, SIGHUP) != 0) {
			perror("kill");
		}
	}
}
