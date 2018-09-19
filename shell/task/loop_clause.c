#include <stdbool.h>
#include <stdlib.h>
#include "shell/task.h"
#include "shell/tasks.h"

struct task_loop_clause {
	struct task task;
	struct {
		struct mrsh_array *condition, *body;
	} ast;
	struct {
		struct task *condition, *body;
	} tasks;
	bool until;
	int last_body_status;
};

static void task_loop_clause_destroy(struct task *task) {
	struct task_loop_clause *tlc = (struct task_loop_clause *)task;
	task_destroy(tlc->tasks.condition);
	task_destroy(tlc->tasks.body);
	free(tlc);
}

static int task_loop_clause_poll(struct task *task, struct context *ctx) {
	struct task_loop_clause *tlc = (struct task_loop_clause *)task;

	while (true) {
		if (tlc->tasks.condition) {
			int condition_status = task_poll(tlc->tasks.condition, ctx);
			if (condition_status < 0) {
				return condition_status;
			} else if (condition_status > 0 && !tlc->until) {
				return tlc->last_body_status;
			} else if (condition_status == 0 && tlc->until) {
				return tlc->last_body_status;
			}
			task_destroy(tlc->tasks.condition);
			tlc->tasks.condition = NULL;
		}

		int body_status = task_poll(tlc->tasks.body, ctx);
		if (body_status < 0) {
			return body_status;
		} else {
			tlc->last_body_status = body_status;
			task_destroy(tlc->tasks.body);
			tlc->tasks.condition =
				task_for_command_list_array(tlc->ast.condition);
			tlc->tasks.body = task_for_command_list_array(tlc->ast.body);
		}
	}
}

static const struct task_interface task_loop_clause_impl = {
	.destroy = task_loop_clause_destroy,
	.poll = task_loop_clause_poll,
};

struct task *task_loop_clause_create(struct mrsh_array *condition,
		struct mrsh_array *body, bool until) {
	struct task_loop_clause *tlc = calloc(1, sizeof(struct task_loop_clause));
	task_init(&tlc->task, &task_loop_clause_impl);
	tlc->ast.condition = condition;
	tlc->ast.body = body;
	tlc->tasks.condition = task_for_command_list_array(condition);
	tlc->tasks.body = task_for_command_list_array(body);
	tlc->until = until;
	return &tlc->task;
}
