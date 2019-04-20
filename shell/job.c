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

bool job_init_process(struct mrsh_state *state) {
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

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
		(array->len - i - 1) * sizeof(void *));
	--array->len;
}

struct job *job_create(struct mrsh_state *state, pid_t pgid) {
	struct job *job = calloc(1, sizeof(struct job));
	job->state = state;
	job->pgid = pgid;
	mrsh_array_add(&state->jobs, job);
	return job;
}

void job_destroy(struct job *job) {
	if (job == NULL) {
		return;
	}

	struct mrsh_state *state = job->state;
	for (size_t i = 0; i < state->jobs.len; ++i) {
		if (state->jobs.data[i] == job) {
			array_remove(&state->jobs, i);
			break;
		}
	}
	mrsh_array_finish(&job->processes);
	free(job);
}

void job_add_process(struct job *job, struct process *proc) {
	mrsh_array_add(&job->processes, proc);
}

void job_notify(struct mrsh_state *state, pid_t pid, int stat) {
	process_notify(pid, stat);

	for (size_t i = 0; i < state->jobs.len; ++i) {
		struct job *job = state->jobs.data[i];
		for (ssize_t j = 0; j < (ssize_t)job->processes.len; ++j) {
			struct process *proc = job->processes.data[j];
			if (proc->finished) {
				array_remove(&job->processes, j);
				j -= 1;
			}
		}

		if (job->processes.len == 0) {
			job->finished = true;
		}
	}
}
