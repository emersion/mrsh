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
	int exit_status;
	int loop_num;
};

static void task_for_clause_destroy(struct task *task) {
	struct task_for_clause *tfc = (struct task_for_clause *)task;
	task_destroy(tfc->tasks.word);
	task_destroy(tfc->tasks.body);
	free(tfc);
}

static int task_for_clause_poll(struct task *task, struct context *ctx) {
	struct task_for_clause *tfc = (struct task_for_clause *)task;
	if (tfc->loop_num == -1) {
		tfc->loop_num = ++ctx->state->nloops;
	}

	while (true) {
		int status;

		if (tfc->tasks.body) {
			/* wait for body */
			status = task_poll(tfc->tasks.body, ctx);
			if (status == TASK_STATUS_INTERRUPTED) {
				goto interrupt;
			} else if (status < 0) {
				return status;
			}
			task_destroy(tfc->tasks.body);
			tfc->tasks.body = NULL;
		} else if (tfc->tasks.word) {
			/* wait for word */
			status = task_poll(tfc->tasks.word, ctx);
			if (status == TASK_STATUS_INTERRUPTED) {
				goto interrupt;
			} else if (status < 0) {
				return status;
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
				goto exit;
			}
			struct mrsh_word **word_ptr =
				(struct mrsh_word **)&tfc->ast.word_list->data[tfc->index++];
			tfc->tasks.word = task_word_create(
				word_ptr, TILDE_EXPANSION_NAME);
		}
		continue;

interrupt:
		if (ctx->state->nloops < tfc->loop_num) {
			/* break to parent loop */
			return status;
		}
		if (ctx->state->branch_control == MRSH_BRANCH_BREAK) {
			tfc->exit_status = 0;
			goto exit;
		} else if (ctx->state->branch_control == MRSH_BRANCH_CONTINUE) {
			task_destroy(tfc->tasks.body);
			tfc->tasks.body = NULL;
		} else {
			assert(0 && "Unknown task interruption cause");
		}
	}

exit:
	--ctx->state->nloops;
	return tfc->exit_status;
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
	tfc->exit_status = 0;
	tfc->loop_num = -1;
	return &tfc->task;
}
