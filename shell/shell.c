#include <assert.h>
#include <stdlib.h>
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

void mrsh_state_finish(struct mrsh_state *state) {
	mrsh_hashtable_for_each(&state->variables,
		state_string_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->variables);
	mrsh_hashtable_for_each(&state->aliases,
		state_string_finish_iterator, NULL);
	mrsh_hashtable_finish(&state->aliases);
	for (int i = 0; i < state->argc; ++i) {
		free(state->argv[i]);
	}
	free(state->argv);
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

	struct context ctx = {
		.state = state,
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	int ret = task_run(task, &ctx);
	task_destroy(task);
	return ret;
}
