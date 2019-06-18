#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include "shell/task.h"

struct task_loop_clause {
	struct task task;
	struct {
		const struct mrsh_array *condition, *body;
	} ast;
	struct {
		struct task *condition, *body;
	} tasks;
	bool until;
	int exit_status;
	int loop_num;
};

static void task_loop_clause_destroy(struct task *task) {
	struct task_loop_clause *tlc = (struct task_loop_clause *)task;
	task_destroy(tlc->tasks.condition);
	task_destroy(tlc->tasks.body);
	free(tlc);
}

static int task_loop_clause_poll(struct task *task, struct context *ctx) {
	struct task_loop_clause *tlc = (struct task_loop_clause *)task;
	if (tlc->loop_num == -1) {
		tlc->loop_num = ++ctx->state->nloops;
	}

	while (ctx->state->exit == -1) {
		int status;

		if (tlc->tasks.condition) {
			status = task_poll(tlc->tasks.condition, ctx);
			if (status == TASK_STATUS_INTERRUPTED) {
				goto interrupt;
			} else if (status < 0) {
				return status;
			} else if (status > 0 && !tlc->until) {
				goto exit;
			} else if (status == 0 && tlc->until) {
				goto exit;
			}
			task_destroy(tlc->tasks.condition);
			tlc->tasks.condition = NULL;
		}

		status = task_poll(tlc->tasks.body, ctx);
		if (status == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (status < 0) {
			return status;
		}

resume:
		tlc->exit_status = status;
		task_destroy(tlc->tasks.body);
		tlc->tasks.condition =
			task_for_command_list_array(tlc->ast.condition);
		tlc->tasks.body = task_for_command_list_array(tlc->ast.body);
		continue;

interrupt:
		if (ctx->state->nloops < tlc->loop_num) {
			/* break to parent loop */
			return status;
		}
		if (ctx->state->branch_control == MRSH_BRANCH_BREAK) {
			tlc->exit_status = 0;
			goto exit;
		} else if (ctx->state->branch_control == MRSH_BRANCH_CONTINUE) {
			goto resume;
		} else {
			assert(0 && "Unknown task interruption cause");
		}
	}

	--ctx->state->nloops;
	return ctx->state->exit;

exit:
	--ctx->state->nloops;
	return tlc->exit_status;
}

static const struct task_interface task_loop_clause_impl = {
	.destroy = task_loop_clause_destroy,
	.poll = task_loop_clause_poll,
};

struct task *task_loop_clause_create(const struct mrsh_array *condition,
		const struct mrsh_array *body, bool until) {
	struct task_loop_clause *tlc = calloc(1, sizeof(struct task_loop_clause));
	task_init(&tlc->task, &task_loop_clause_impl);
	tlc->ast.condition = condition;
	tlc->ast.body = body;
	tlc->tasks.condition = task_for_command_list_array(condition);
	tlc->tasks.body = task_for_command_list_array(body);
	tlc->until = until;
	tlc->loop_num = -1;
	return &tlc->task;
}
