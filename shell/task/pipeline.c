#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/task.h"

struct task_pipeline {
	struct task task;
	struct mrsh_array children;
	struct context child_ctx;
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

static bool task_pipeline_start(struct task *task, struct context *ctx) {
	struct task_pipeline *tp = (struct task_pipeline *)task;

	// Save stdin/stdout so we can restore them after the pipeline
	// We don't need to do this if there's only one command in the pipeline
	int dup_stdin = -1, dup_stdout = -1;
	if (tp->children.len > 1) {
		dup_stdin = dup(STDIN_FILENO);
		dup_stdout = dup(STDOUT_FILENO);
		if (dup_stdin < 0 || dup_stdout < 0) {
			fprintf(stderr, "failed to duplicate stdin or stdout: %s\n",
				strerror(errno));
			return false;
		}
	}

	int last_stdout = -1;
	for (size_t i = 0; i < tp->children.len; ++i) {
		if (i > 0) {
			if (dup2(last_stdout, STDIN_FILENO) < 0) {
				fprintf(stderr, "failed to duplicate stdin: %s\n",
					strerror(errno));
				return false;
			}
			close(last_stdout);
		}

		int new_stdout = dup_stdout; // Restore stdout if it's the last command
		if (i < tp->children.len - 1) {
			int fds[2];
			if (pipe(fds) != 0) {
				fprintf(stderr, "failed to pipe(): %s\n", strerror(errno));
				return false;
			}

			// We'll use the write end of the pipe as stdout, the read end will
			// be used as stdin by the next command
			last_stdout = fds[0];
			new_stdout = fds[1];
		}
		if (new_stdout >= 0) {
			if (dup2(new_stdout, STDOUT_FILENO) < 0) {
				fprintf(stderr, "failed to duplicate stdout: %s\n",
					strerror(errno));
				return false;
			}
			close(new_stdout);
		}

		struct task *child = tp->children.data[i];
		int ret = task_poll(child, ctx);
		if (ret == TASK_STATUS_ERROR) {
			return false;
		}
	}

	// Restore stdin
	if (dup_stdin >= 0) {
		if (dup2(dup_stdin, STDIN_FILENO) < 0) {
			fprintf(stderr, "failed to restore stdin: %s\n", strerror(errno));
			return false;
		}
		close(dup_stdin);
	}

	return true;
}

static int task_pipeline_poll(struct task *task, struct context *ctx) {
	struct task_pipeline *tp = (struct task_pipeline *)task;

	if (!tp->started) {
		// All child processes should be put into the same process group
		tp->child_ctx = *ctx;
		tp->child_ctx.pgid = 0;

		if (!task_pipeline_start(task, &tp->child_ctx)) {
			return TASK_STATUS_ERROR;
		}
		tp->started = true;
	}

	int ret = 0;
	for (size_t i = 0; i < tp->children.len; ++i) {
		struct task *child = tp->children.data[i];
		ret = task_poll(child, &tp->child_ctx);
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
