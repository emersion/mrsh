#include <stdlib.h>
#include "shell.h"

struct task_bg {
	struct task task;
	struct task *bg;
};

static int task_bg_poll(struct task *task, struct context *ctx) {
	struct task_bg *tb = (struct task_bg *)task;

	int status = task_poll(tb->bg, ctx);
	if (status == TASK_STATUS_ERROR) {
		return status;
	}

	return 0;
}

static const struct task_interface task_bg_impl = {
	.poll = task_bg_poll,
};

struct task *task_bg_create(struct task *bg) {
	struct task_bg *tb = calloc(1, sizeof(struct task_bg));
	task_init(&tb->task, &task_bg_impl);
	tb->bg = bg;
	return &tb->task;
}
