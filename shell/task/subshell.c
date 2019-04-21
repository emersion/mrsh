#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "shell/process.h"
#include "shell/task.h"

struct task_subshell {
	struct task task;
	struct task *subtask;
	bool started;
	struct process *process;
};

static void task_subshell_destroy(struct task *task) {
	struct task_subshell *ts = (struct task_subshell *)task;
	task_destroy(ts->subtask);
	free(ts);
}

static bool task_subshell_start(struct task_subshell *ts, struct context *ctx) {
	pid_t pid = subshell_fork(ctx, &ts->process);
	if (pid < 0) {
		return false;
	} else if (pid == 0) {
		int ret = task_run(ts->subtask, ctx);
		if (ret < 0) {
			assert(ret == TASK_STATUS_ERROR);
			fprintf(stderr, "failed to run task: %s\n", strerror(errno));
			exit(127);
		}

		if (ctx->state->exit >= 0) {
			exit(ctx->state->exit);
		}
		exit(ret);
	}

	return true;
}

static int task_subshell_poll(struct task *task, struct context *ctx) {
	struct task_subshell *ts = (struct task_subshell *)task;

	if (!ts->started) {
		if (!task_subshell_start(ts, ctx)) {
			return TASK_STATUS_ERROR;
		}
		ts->started = true;
	}

	return process_poll(ts->process);
}

static const struct task_interface task_subshell_impl = {
	.destroy = task_subshell_destroy,
	.poll = task_subshell_poll,
};

struct task *task_subshell_create(struct task *subtask) {
	struct task_subshell *ts = calloc(1, sizeof(struct task_subshell));
	task_init(&ts->task, &task_subshell_impl);
	ts->subtask = subtask;
	return &ts->task;
}
