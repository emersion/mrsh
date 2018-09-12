#include <unistd.h>
#include <stdlib.h>
#include "shell/task.h"

struct task_pipeline {
	struct task task;
	struct mrsh_array children;
	bool started;
};

static void task_pipeline_destroy(struct task *task) {
	struct task_pipeline *tp = (struct task_pipeline *)task;
	for (size_t i = 0; i < tp->children.len; ++i) {
		struct task *child = tp->children.data[i];
		task_destroy(child);
	}
	mrsh_array_finish(&tp->children);
	free(tp);
}

static bool task_pipeline_start(struct task *task, struct context *parent_ctx) {
	struct task_pipeline *tp = (struct task_pipeline *)task;

	struct context ctx = { .state = parent_ctx->state };

	int last_stdout = -1;
	for (size_t i = 0; i < tp->children.len; ++i) {
		ctx.stdin_fileno = -1;
		ctx.stdout_fileno = -1;

		if (i > 0) {
			ctx.stdin_fileno = last_stdout;
		} else {
			ctx.stdin_fileno = parent_ctx->stdin_fileno;
		}

		if (i < tp->children.len - 1) {
			int fds[2];
			pipe(fds);
			ctx.stdout_fileno = fds[1];
			last_stdout = fds[0];
		} else {
			ctx.stdout_fileno = parent_ctx->stdout_fileno;
		}

		struct task *child = tp->children.data[i];
		int ret = task_poll(child, &ctx);
		if (ret == TASK_STATUS_ERROR) {
			return false;
		}
	}

	return true;
}

static int task_pipeline_poll(struct task *task, struct context *ctx) {
	struct task_pipeline *tp = (struct task_pipeline *)task;

	if (!tp->started) {
		if (!task_pipeline_start(task, ctx)) {
			return TASK_STATUS_ERROR;
		}
		tp->started = true;
	}

	int ret = 0;
	for (size_t i = 0; i < tp->children.len; ++i) {
		struct task *child = tp->children.data[i];
		ret = task_poll(child, ctx);
		if (ret < 0) {
			return ret;
		}
	}

	return ret;
}

static const struct task_interface task_pipeline_impl = {
	.destroy = task_pipeline_destroy,
	.poll = task_pipeline_poll,
};

struct task *task_pipeline_create(void) {
	struct task_pipeline *tp = calloc(1, sizeof(struct task_pipeline));
	task_init(&tp->task, &task_pipeline_impl);
	return &tp->task;
}

void task_pipeline_add(struct task *task, struct task *child) {
	struct task_pipeline *tp = (struct task_pipeline *)task;
	mrsh_array_add(&tp->children, child);
}
