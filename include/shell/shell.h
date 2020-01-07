#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <mrsh/shell.h>
#include <termios.h>
#include "job.h"
#include "process.h"
#include "shell/trap.h"

struct mrsh_variable {
	char *value;
	uint32_t attribs; // enum mrsh_variable_attrib
};

struct mrsh_function {
	struct mrsh_command *body;
};

enum mrsh_branch_control {
	MRSH_BRANCH_BREAK,
	MRSH_BRANCH_CONTINUE,
	MRSH_BRANCH_RETURN,
	MRSH_BRANCH_EXIT,
};

struct mrsh_call_frame_priv {
	struct mrsh_call_frame pub;

	enum mrsh_branch_control branch_control;
	int nloops;
};

struct mrsh_trap {
	bool set;
	enum mrsh_trap_action action;
	struct mrsh_program *program;
};

struct mrsh_state_priv {
	struct mrsh_state pub;

	int term_fd;
	struct mrsh_array processes;
	struct mrsh_hashtable aliases; // char *
	struct mrsh_hashtable variables; // struct mrsh_variable *
	struct mrsh_hashtable functions; // struct mrsh_function *

	bool job_control;
	pid_t pgid;
	struct termios term_modes;
	struct sigaction *saved_sigactions;
	struct mrsh_array jobs; // struct mrsh_job *
	struct mrsh_job *foreground_job;

	struct mrsh_trap traps[MRSH_NSIG];

	// TODO: move this to context
	bool child; // true if we're not the main shell process
};

/**
 * A context holds state information and per-job information. A context is
 * guaranteed to be shared between all members of a job.
 */
struct mrsh_context {
	struct mrsh_state *state;
	// When executing a pipeline, this is set to the job created for the
	// pipeline
	struct mrsh_job *job;
	// When executing an asynchronous list, this is set to true
	bool background;
};

void function_destroy(struct mrsh_function *fn);

struct mrsh_call_frame_priv *call_frame_get_priv(struct mrsh_call_frame *frame);

struct mrsh_state_priv *state_get_priv(struct mrsh_state *state);
void push_frame(struct mrsh_state *state, int argc, const char *argv[]);
void pop_frame(struct mrsh_state *state);

#endif
