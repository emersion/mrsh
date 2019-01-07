#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "shell/job.h"
#include "shell/task.h"

void task_init(struct task *task, const struct task_interface *impl) {
	assert(impl->poll);
	task->impl = impl;
	task->status = TASK_STATUS_WAIT;
}

void task_destroy(struct task *task) {
	if (task == NULL) {
		return;
	}

	if (task->impl->destroy) {
		task->impl->destroy(task);
	} else {
		free(task);
	}
}

int task_poll(struct task *task, struct context *ctx) {
	if (task->status == TASK_STATUS_WAIT) {
		task->status = task->impl->poll(task, ctx);
	}
	return task->status;
}

int task_run(struct task *task, struct context *ctx) {
	while (true) {
		int ret = task_poll(task, ctx);
		if (ret != TASK_STATUS_WAIT) {
			ctx->state->last_status = ret;
			if (ret != EXIT_SUCCESS
					&& (ctx->state->options & MRSH_OPT_ERREXIT)) {
				ctx->state->exit = ret;
			}
			return ret;
		}

		struct job *job = job_foreground();
		if (job == NULL) {
			return EXIT_SUCCESS;
		}
		if (!job_wait(job)) {
			return -1;
		}
	}
}
