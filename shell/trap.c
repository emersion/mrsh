#define _POSIX_C_SOURCE 1
#include <assert.h>
#include <signal.h>
#include "shell/shell.h"
#include "shell/trap.h"

static const int ignored_job_control_sigs[] = {
	SIGINT,
	SIGQUIT,
	SIGTSTP,
	SIGTTIN,
	SIGTTOU,
};
static const size_t ignored_job_control_sigs_len =
	sizeof(ignored_job_control_sigs) / sizeof(ignored_job_control_sigs[0]);

static int pending_sigs[MRSH_NSIG] = {0};

static void handle_signal(int sig) {
	assert(sig < MRSH_NSIG);
	pending_sigs[sig]++;
}

bool set_trap(struct mrsh_state *state, int sig, enum mrsh_trap_action action,
		struct mrsh_program *program) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	assert(action == MRSH_TRAP_CATCH || program == NULL);

	struct mrsh_trap *trap = &priv->traps[sig];

	if (sig != 0) {
		if (!trap->set && !state->interactive) {
			// Signals that were ignored on entry to a non-interactive shell
			// cannot be trapped or reset
			struct sigaction sa;
			if (sigaction(sig, NULL, &sa) != 0) {
				perror("failed to get current signal action: sigaction");
				return false;
			}
			if (sa.sa_handler == SIG_IGN) {
				fprintf(stderr, "cannot trap signal %d: "
					"ignored on non-interactive shell entry\n", sig);
				return false;
			}
		}

		if (action == MRSH_TRAP_DEFAULT && priv->job_control) {
			// When job control is enabled, some signals are ignored by default
			for (size_t i = 0; i < ignored_job_control_sigs_len; i++) {
				if (sig == ignored_job_control_sigs[i]) {
					action = MRSH_TRAP_IGNORE;
					break;
				}
			}
		}

		struct sigaction sa = {0};
		switch (action) {
		case MRSH_TRAP_DEFAULT:
			sa.sa_handler = SIG_DFL;
			break;
		case MRSH_TRAP_IGNORE:
			sa.sa_handler = SIG_IGN;
			break;
		case MRSH_TRAP_CATCH:
			sa.sa_handler = handle_signal;
			break;
		}

		if (sigaction(sig, &sa, NULL) < 0) {
			perror("failed to set signal action: sigaction");
			return false;
		}
	}

	trap->set = true;
	trap->action = action;
	mrsh_program_destroy(trap->program);
	trap->program = program;

	return true;
}

bool set_job_control_traps(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	for (size_t i = 0; i < ignored_job_control_sigs_len; i++) {
		int sig = ignored_job_control_sigs[i];
		struct mrsh_trap *trap = &priv->traps[i];

		struct sigaction sa = {0};
		if (priv->job_control) {
			sa.sa_handler = SIG_IGN;
		} else {
			sa.sa_handler = SIG_DFL;
		}
		if (sigaction(sig, &sa, NULL) < 0) {
			perror("sigaction");
			return false;
		}

		trap->set = false;
		trap->action = MRSH_TRAP_DEFAULT;
		mrsh_program_destroy(trap->program);
		trap->program = NULL;
	}
	return true;
}

bool reset_caught_traps(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	for (size_t i = 0; i < MRSH_NSIG; i++) {
		struct mrsh_trap *trap = &priv->traps[i];
		if (trap->set && trap->action != MRSH_TRAP_IGNORE) {
			if (!set_trap(state, i, MRSH_TRAP_DEFAULT, NULL)) {
				return false;
			}
		}
	}

	return true;
}

bool run_pending_traps(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);
	static bool in_trap = false;

	if (in_trap) {
		return true;
	}
	in_trap = true;

	int last_status = state->last_status;

	for (size_t i = 0; i < MRSH_NSIG; i++) {
		struct mrsh_trap *trap = &priv->traps[i];
		while (pending_sigs[i] > 0) {
			if (!trap->set || trap->action != MRSH_TRAP_CATCH ||
					trap->program == NULL) {
				break;
			}

			int ret = mrsh_run_program(state, trap->program);
			if (ret < 0) {
				return false;
			}

			pending_sigs[i]--;
			state->last_status = last_status; // Restore "$?"
		}

		pending_sigs[i] = 0;
	}

	in_trap = false;
	return true;
}

bool run_exit_trap(struct mrsh_state *state) {
	pending_sigs[0]++;
	return run_pending_traps(state);
}
