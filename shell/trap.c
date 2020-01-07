#define _POSIX_C_SOURCE 1
#include <assert.h>
#include <signal.h>
#include "shell/shell.h"
#include "shell/trap.h"

static int pending_sigs[MRSH_NSIG] = {0};

static void handle_signal(int sig) {
	assert(sig < MRSH_NSIG);
	pending_sigs[sig]++;
}

bool set_trap(struct mrsh_state *state, int sig, enum mrsh_trap_action action,
		struct mrsh_program *program) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	assert(action == MRSH_TRAP_CATCH || program == NULL);

	if (sig != 0) {
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
			perror("sigaction");
			return false;
		}
	}

	struct mrsh_trap *trap = &priv->traps[sig];
	trap->set = true;
	trap->action = action;
	mrsh_program_destroy(trap->program);
	trap->program = program;

	return true;
}

bool run_pending_traps(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);
	static bool in_trap = false;

	// TODO: save and restore $?

	if (in_trap) {
		return true;
	}
	in_trap = true;

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
