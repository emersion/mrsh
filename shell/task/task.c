#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
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

static void destroy_finished_jobs(struct mrsh_state *state) {
	for (ssize_t i = 0; i < (ssize_t)state->jobs.len; ++i) {
		struct mrsh_job *job = state->jobs.data[i];
		if (job_terminated(job)) {
			job_destroy(job);
			--i;
		}
	}
}

int task_run(struct task *task, struct context *ctx) {
	while (true) {
		int ret = task_poll(task, ctx);
		if (ret != TASK_STATUS_WAIT) {
			ctx->state->last_status = ret;
			if (ret != 0
					&& (ctx->state->options & MRSH_OPT_ERREXIT)) {
				ctx->state->exit = ret;
			}

			// Put the shell back in the foreground
			tcsetpgrp(ctx->state->fd, ctx->state->pgid);
			// Restore the shell’s terminal modes
			// tcgetattr(ctx->state->fd, &job->term_modes); // TODO
			tcsetattr(ctx->state->fd, TCSADRAIN, &ctx->state->term_modes);

			destroy_finished_jobs(ctx->state);

			return ret;
		}

		destroy_finished_jobs(ctx->state);

		errno = 0;
		int stat;
		pid_t pid = waitpid(-1, &stat, 0);
		if (pid == -1) {
			if (errno == EINTR) {
				continue;
			}
			fprintf(stderr, "failed to waitpid(): %s\n", strerror(errno));
			return -1;
		}

		update_job(ctx->state, pid, stat);
	}
}
