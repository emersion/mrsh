#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <mrsh/hashtable.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"
#include "shell/tasks.h"

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
	state->interactive = isatty(STDIN_FILENO);
	state->options = state->interactive ? MRSH_OPT_INTERACTIVE : 0;
	state->input = stdin;
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

static void function_destroy(struct mrsh_function *fn) {
	if (!fn) {
		return;
	}
	free(fn);
}

static void state_fn_finish_iterator(const char *key, void *value, void *_) {
	function_destroy((struct mrsh_function *)value);
}

void mrsh_state_finish(struct mrsh_state *state) {
	mrsh_hashtable_for_each(&state->variables, state_var_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->variables);
	mrsh_hashtable_for_each(&state->functions, state_fn_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->functions);
	mrsh_hashtable_for_each(&state->aliases,
		state_string_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->aliases);
	for (int i = 0; i < state->argc; ++i) {
		free(state->argv[i]);
	}
	free(state->argv);
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
