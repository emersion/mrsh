#include <stdlib.h>
#include "shell.h"

struct task_binop {
	struct task task;
	enum mrsh_binop_type type;
	struct task *left, *right;
};

static int task_binop_poll(struct task *task, struct context *ctx) {
	struct task_binop *tb = (struct task_binop *)task;

	int left_status = task_poll(tb->left, ctx);
	if (left_status < 0) {
		return left_status;
	}

	switch (tb->type) {
	case MRSH_BINOP_AND:
		if (left_status != 0) {
			return left_status;
		}
		break;
	case MRSH_BINOP_OR:
		if (left_status == 0) {
			return 0;
		}
		break;
	}

	return task_poll(tb->right, ctx);
}

static const struct task_interface task_binop_impl = {
	.poll = task_binop_poll,
};

struct task *task_binop_create(enum mrsh_binop_type type,
		struct task *left, struct task *right) {
	struct task_binop *tb = calloc(1, sizeof(struct task_binop));
	task_init(&tb->task, &task_binop_impl);
	tb->type = type;
	tb->left = left;
	tb->right = right;
	return &tb->task;
}
