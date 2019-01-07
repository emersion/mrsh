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

static const int ignored_signals[] = {
	SIGINT,
	SIGQUIT,
	SIGTSTP,
	SIGTTIN,
	SIGTTOU,
};

static const size_t IGNORED_SIGNALS_LEN =
		sizeof(ignored_signals) / sizeof(ignored_signals[0]);

// TODO: these shouldn't be globals
struct mrsh_array jobs = { 0 };
static bool job_control_enabled = false;
static struct job *foreground_job = NULL;

bool mrsh_set_job_control(struct mrsh_state *state, bool enabled) {
	assert(state->fd >= 0);

	if (job_control_enabled == enabled) {
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
			sigaction(ignored_signals[i], &sa, NULL);
		}

		// Put ourselves in our own process group
		state->pgid = getpid();
		if (setpgid(state->pgid, state->pgid) < 0) {
			return false;
		}

		// Grab control of the terminal
		tcsetpgrp(state->fd, state->pgid);
		// Save default terminal attributes for shell
		tcgetattr(state->fd, &state->term_modes);
	} else {
		return false; // TODO
	}

	job_control_enabled = enabled;
	return true;
}

bool job_child_init(struct mrsh_state *state, bool foreground) {
	// TODO: don't always create a new process group
	pid_t pgid = getpid();
	if (setpgid(pgid, pgid) < 0) {
		return false;
	}

	if (foreground) {
		tcsetpgrp(state->fd, pgid);
	}

	struct sigaction sa = { .sa_handler = SIG_DFL };
	sigemptyset(&sa.sa_mask);
	for (size_t i = 0; i < IGNORED_SIGNALS_LEN; ++i) {
		sigaction(ignored_signals[i], &sa, NULL);
	}

	return true;
}

struct job *job_create(struct mrsh_state *state) {
	struct job *job = calloc(1, sizeof(struct job));
	job->state = state;
	job->pgid = 0;
	mrsh_array_add(&jobs, job);
	return job;
}

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
			(array->len - i - 1) * sizeof(void *));
	--array->len;
}

static void job_remove(struct job *job) {
	assert(foreground_job != job);

	for (size_t i = 0; i < jobs.len; ++i) {
		if (jobs.data[i] == job) {
			array_remove(&jobs, i);
			break;
		}
	}
}

void job_destroy(struct job *job) {
	if (job == NULL) {
		return;
	}
	job_set_foreground(job, false);
	job_remove(job);
	free(job);
}

void job_add_process(struct job *job, pid_t pid) {
	if (job->pgid == 0) {
		job->pgid = pid;
	}

	setpgid(pid, job->pgid);
}

static struct job *job_from_pid(pid_t pid) {
	for (size_t i = 0; i < jobs.len; ++i) {
		struct job *job = jobs.data[i];
		// TODO: search in process list
		if (job->pgid == pid) {
			return job;
		}
	}
	return NULL;
}

bool job_set_foreground(struct job *job, bool foreground) {
	struct mrsh_state *state = job->state;

	bool is_foreground = foreground_job == job;
	if (is_foreground == foreground) {
		return true;
	}

	if (foreground) {
		if (foreground_job != NULL) {
			job_set_foreground(foreground_job, false);
		}

		tcsetpgrp(state->fd, job->pgid);

		// TODO: only do this if we want to continue this job
		//tcsetattr(state->fd, TCSADRAIN, &job->term_modes);
		foreground_job = job;
	} else {
		if (tcgetpgrp(state->fd) == job->pgid) {
			tcsetpgrp(state->fd, state->pgid);

			tcgetattr(state->fd, &job->term_modes);
			tcsetattr(state->fd, TCSADRAIN, &state->term_modes);
		}

		if (foreground_job == job) {
			foreground_job = NULL;
		}
	}

	return true;
}

bool job_continue(struct job *job) {
	// TODO: mark all processes as non-stopped
	return kill(-job->pgid, SIGCONT) >= 0;
}

struct job *job_foreground(void) {
	return foreground_job;
}

bool job_wait(struct job *job) {
	while (true) {
		int stat;
		pid_t pid;
		do {
			pid = waitpid(-1, &stat, WUNTRACED);
		} while (pid == -1 && errno == EINTR);

		if (pid == -1) {
			fprintf(stderr, "failed to waitpid(): %s\n", strerror(errno));
			return false;
		}
		printf("waitpid() = %d, %d, WIFSTOPPED=%d, WIFEXITED=%d, WIFSIGNALED=%d\n",
			pid, stat, WIFSTOPPED(stat), WIFEXITED(stat), WIFSIGNALED(stat)); // TODO: remove me

		process_notify(pid, stat);

		struct job *waited_job = job_from_pid(pid);
		if (waited_job == NULL) {
			continue;
		}

		// TODO: only works if job has only one process
		// TODO: figure out when to destroy jobs
		if (WIFSTOPPED(stat)) {
			job_set_foreground(job, false);
		} else if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
			job_set_foreground(job, false);
			job_remove(job);
		} else {
			assert(false);
		}

		if (job == waited_job) {
			return true;
		}
	}
}
