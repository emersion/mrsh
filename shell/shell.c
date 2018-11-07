#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <mrsh/hashtable.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
	state->interactive = isatty(STDIN_FILENO);
	state->options = state->interactive ? MRSH_OPT_INTERACTIVE : 0;
	state->args = calloc(1, sizeof(struct mrsh_call_frame));
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

void function_destroy(struct mrsh_function *fn) {
	if (!fn) {
		return;
	}
	mrsh_command_destroy(fn->body);
	free(fn);
}

static void state_fn_finish_iterator(const char *key, void *value, void *_) {
	function_destroy((struct mrsh_function *)value);
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
		exit(EXIT_FAILURE);
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

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	struct task *task = task_for_command_list_array(&prog->body);

	struct context ctx = {
		.state = state,
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	int ret = task_run(task, &ctx);
	task_destroy(task);
	return ret;
}

int mrsh_run_word(struct mrsh_state *state, struct mrsh_word **word) {
	struct task *task = task_word_create(word, TILDE_EXPANSION_NAME);

	int last_status = state->last_status;
	struct context ctx = {
		.state = state,
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	int ret = task_run(task, &ctx);
	task_destroy(task);
	state->last_status = last_status;
	return ret;
}
