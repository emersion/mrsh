#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <mrsh/shell.h>
#include "job.h"
#include "process.h"

/**
 * A context holds state information and per-job information. A context is
 * guaranteed to be shared between all members of a job.
 */
struct context {
	struct mrsh_state *state;
	// When executing a pipeline, this is set to the job created for the
	// pipeline
	struct mrsh_job *job;
	// When executing an asynchronous list, this is set to true
	bool background;
};

/**
 * Like fork(2), but keeps track of the child.
 *
 * This function returns twice on success: once with 0 in the child, once with
 * a > 0 value in the parent. On error, it returns -1 once.
 */
pid_t subshell_fork(struct context *ctx, struct process **process_ptr);

#endif
