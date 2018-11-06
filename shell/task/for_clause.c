#define _POSIX_C_SOURCE 200809L
#include "shell/task.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct task_for_clause {
	struct task task;
	struct {
		const char *name;
		const struct mrsh_array *word_list, *body;
	} ast;
	struct {
		struct task *word, *body;
	} tasks;
	size_t index;
	int last_body_status;
};

static void task_for_clause_destroy(struct task *task) {
	struct task_for_clause *tfc = (struct task_for_clause *)task;
	task_destroy(tfc->tasks.word);
	task_destroy(tfc->tasks.body);
	free(tfc);
}

static int task_for_clause_poll(struct task *task, struct context *ctx) {
	struct task_for_clause *tfc = (struct task_for_clause *)task;
	while (true) {
		if (tfc->tasks.body) {
			/* wait for body */
			int body_status = task_poll(tfc->tasks.body, ctx);
			if (body_status < 0) {
				return body_status;
			}
			task_destroy(tfc->tasks.body);
			tfc->tasks.body = NULL;
		} else if (tfc->tasks.word) {
			/* wait for word */
			int word_status = task_poll(tfc->tasks.word, ctx);
			if (word_status < 0) {
				return word_status;
			}
			struct mrsh_word_string *word = (struct mrsh_word_string *)
				tfc->ast.word_list->data[tfc->index - 1];
			mrsh_env_set(ctx->state, tfc->ast.name, word->str,
				MRSH_VAR_ATTRIB_NONE);
			task_destroy(tfc->tasks.word);
			tfc->tasks.word = NULL;
			tfc->tasks.body =
				task_for_command_list_array(tfc->ast.body);
		} else {
			/* create a new word */
			if (tfc->index == tfc->ast.word_list->len) {
				return 0;
			}
			struct mrsh_word **word_ptr =
				(struct mrsh_word **)&tfc->ast.word_list->data[tfc->index++];
			tfc->tasks.word = task_word_create(
				word_ptr, TILDE_EXPANSION_NAME);
		}
	}
}

static const struct task_interface task_for_clause_impl = {
	.destroy = task_for_clause_destroy,
	.poll = task_for_clause_poll,
};

struct task *task_for_clause_create(const char *name,
		const struct mrsh_array *word_list, const struct mrsh_array *body) {
	struct task_for_clause *tfc = calloc(1, sizeof(struct task_for_clause));
	task_init(&tfc->task, &task_for_clause_impl);
	tfc->ast.name = name;
	tfc->ast.word_list = word_list;
	tfc->ast.body = body;
	tfc->index = 0;
	return &tfc->task;
}
