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
