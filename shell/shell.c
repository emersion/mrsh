#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/hashtable.h>
#include <mrsh/parser.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/shell.h"
#include "shell/process.h"

void mrsh_function_destroy(struct mrsh_function *fn) {
	if (!fn) {
		return;
	}
	mrsh_command_destroy(fn->body);
	free(fn);
}

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
	state->fd = -1;
	state->nloops = 0;
	state->interactive = isatty(STDIN_FILENO);
	state->options = state->interactive ? MRSH_OPT_INTERACTIVE : 0;
	state->args = calloc(1, sizeof(struct mrsh_call_frame));
}

static const char *get_alias(const char *name, void *data) {
	struct mrsh_state *state = data;
	return mrsh_hashtable_get(&state->aliases, name);
}

void mrsh_state_set_parser_alias_func(
		struct mrsh_state *state, struct mrsh_parser *parser) {
	mrsh_parser_set_alias_func(parser, get_alias, state);
}

static void state_string_finish_iterator(const char *key, void *value,
		void *user_data) {
	free(value);
}

static void variable_destroy(struct mrsh_variable *var) {
	if (!var) {
		return;
	}
	free(var->value);
	free(var);
}

static void state_var_finish_iterator(const char *key, void *value,
		void *user_data) {
	variable_destroy((struct mrsh_variable *)value);
}

static void state_fn_finish_iterator(const char *key, void *value, void *_) {
	mrsh_function_destroy((struct mrsh_function *)value);
}

static void call_frame_destroy(struct mrsh_call_frame *args) {
	for (int i = 0; i < args->argc; ++i) {
		free(args->argv[i]);
	}
	free(args->argv);
	free(args);
}

void mrsh_state_finish(struct mrsh_state *state) {
	mrsh_hashtable_for_each(&state->variables, state_var_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->variables);
	mrsh_hashtable_for_each(&state->functions, state_fn_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->functions);
	mrsh_hashtable_for_each(&state->aliases,
		state_string_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->aliases);
	while (state->jobs.len > 0) {
		job_destroy(state->jobs.data[state->jobs.len - 1]);
	}
	mrsh_array_finish(&state->jobs);
	while (state->processes.len > 0) {
		process_destroy(state->processes.data[state->processes.len - 1]);
	}
	mrsh_array_finish(&state->processes);
	struct mrsh_call_frame *args = state->args;
	while (args) {
		struct mrsh_call_frame *prev = args->prev;
		call_frame_destroy(args);
		args = prev;
	}
}

void mrsh_env_set(struct mrsh_state *state,
		const char *key, const char *value, uint32_t attribs) {
	struct mrsh_variable *var = calloc(1, sizeof(struct mrsh_variable));
	if (!var) {
		fprintf(stderr, "Failed to allocate shell variable");
		exit(1);
	}
	var->value = strdup(value);
	var->attribs = attribs;
	struct mrsh_variable *old = mrsh_hashtable_set(&state->variables, key, var);
	variable_destroy(old);
}

void mrsh_env_unset(struct mrsh_state *state, const char *key) {
	variable_destroy(mrsh_hashtable_del(&state->variables, key));
}

const char *mrsh_env_get(struct mrsh_state *state,
		const char *key, uint32_t *attribs) {
	struct mrsh_variable *var = mrsh_hashtable_get(&state->variables, key);
	if (var && attribs) {
		*attribs = var->attribs;
	}
	return var ? var->value : NULL;
}

void mrsh_push_args(struct mrsh_state *state, int argc, const char *argv[]) {
	struct mrsh_call_frame *next = calloc(1, sizeof(struct mrsh_call_frame));
	next->argc = argc;
	next->argv = malloc(sizeof(char *) * argc);
	for (int i = 0; i < argc; ++i) {
		next->argv[i] = strdup(argv[i]);
	}
	next->prev = state->args;
	state->args = next;
}

void mrsh_pop_args(struct mrsh_state *state) {
	struct mrsh_call_frame *args = state->args;
	assert(args->prev != NULL);
	state->args = args->prev;
	call_frame_destroy(args);
}

/**
 * Put the process into its job's process group. This has to be done both in the
 * parent and the child because of potential race conditions.
 */
static struct process *init_child(struct context *ctx, pid_t pid) {
	struct process *proc = process_create(ctx->state, pid);

	if (ctx->state->options & MRSH_OPT_MONITOR) {
		// Create a job for all children processes
		struct mrsh_job *job = job_create(ctx->state);
		job_add_process(job, proc);

		if (ctx->state->interactive && !ctx->background) {
			job_set_foreground(job, true, false);
		}
	}

	return proc;
}

pid_t subshell_fork(struct context *ctx, struct process **process_ptr) {
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		ctx->state->child = true;

		init_child(ctx, getpid());
		if (ctx->state->options & MRSH_OPT_MONITOR) {
			init_job_child_process(ctx->state);
		}

		return 0;
	}

	struct process *proc = init_child(ctx, pid);
	if (process_ptr != NULL) {
		*process_ptr = proc;
	}
	return pid;
}
