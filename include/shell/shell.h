#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <mrsh/shell.h>

/**
 * A context holds state information and per-job information. A context is
 * guaranteed to be shared between all members of a job.
 */
struct context {
	struct mrsh_state *state;

	// Per-job information
	pid_t pgid;
};

#endif
