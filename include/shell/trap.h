#ifndef SHELL_TRAP_H
#define SHELL_TRAP_H

#include <stdbool.h>

struct mrsh_state;
struct mrsh_program;

// TODO: find a POSIX-compatible way to get the max signal value
#define MRSH_NSIG 64

enum mrsh_trap_action {
	MRSH_TRAP_DEFAULT, // SIG_DFL
	MRSH_TRAP_IGNORE, // SIG_IGN
	MRSH_TRAP_CATCH,
};

bool set_trap(struct mrsh_state *state, int sig, enum mrsh_trap_action action,
	struct mrsh_program *program);
bool set_job_control_traps(struct mrsh_state *state);
bool reset_caught_traps(struct mrsh_state *state);
bool run_pending_traps(struct mrsh_state *state);
bool run_exit_trap(struct mrsh_state *state);

#endif
