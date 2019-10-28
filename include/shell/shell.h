#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <mrsh/shell.h>
#include "job.h"
#include "process.h"

struct mrsh_variable {
	char *value;
	uint32_t attribs; // enum mrsh_variable_attrib
};

struct mrsh_function {
	struct mrsh_command *body;
};

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

void function_destroy(struct mrsh_function *fn);

#endif
