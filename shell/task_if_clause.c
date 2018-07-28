#include <stdlib.h>
#include "shell.h"

struct task_if_clause {
	struct task task;
	struct task *condition, *body, *else_part;
};

static void task_if_clause_destroy(struct task *task) {
	struct task_if_clause *tic = (struct task_if_clause *)task;
	task_destroy(tic->condition);
	task_destroy(tic->body);
	task_destroy(tic->else_part);
	free(tic);
}

static int task_if_clause_poll(struct task *task, struct context *ctx) {
	struct task_if_clause *tic = (struct task_if_clause *)task;

	int condition_status = task_poll(tic->condition, ctx);
	if (condition_status < 0) {
		return condition_status;
	}

	if (condition_status == 0) {
		return task_poll(tic->body, ctx);
	} else {
		if (tic->else_part == NULL) {
			return 0;
		}
		return task_poll(tic->else_part, ctx);
	}
}

static const struct task_interface task_if_clause_impl = {
	.destroy = task_if_clause_destroy,
	.poll = task_if_clause_poll,
};

struct task *task_if_clause_create(struct task *condition, struct task *body,
		struct task *else_part) {
	struct task_if_clause *tic = calloc(1, sizeof(struct task_if_clause));
	task_init(&tic->task, &task_if_clause_impl);
	tic->condition = condition;
	tic->body = body;
	tic->else_part = else_part;
	return &tic->task;
}
