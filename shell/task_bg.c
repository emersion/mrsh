#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell.h"

struct task_bg {
	struct task task;
	struct task *bg;
	bool started;
};

static int fork_detached(void) {
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		pid_t child_pid = fork();
		if (child_pid < 0) {
			fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
			return -1;
		} else if (child_pid == 0) {
			return 0;
		} else {
			exit(EXIT_SUCCESS);
		}
	} else {
		if (waitpid(pid, NULL, 0) == -1) {
			fprintf(stderr, "failed to waitpid(): %s\n", strerror(errno));
			return -1;
		}

		return 1;
	}
}

static bool task_bg_start(struct task *task, struct context *ctx) {
	struct task_bg *tb = (struct task_bg *)task;

	// Start a subshell
	int ret = fork_detached();
	if (ret < 0) {
		return false;
	} else if (ret == 0) {
		if (ctx->stdin_fileno >= 0) {
			dup2(ctx->stdin_fileno, STDIN_FILENO);
		}
		if (ctx->stdout_fileno >= 0) {
			dup2(ctx->stdout_fileno, STDOUT_FILENO);
		}

		int ret = task_run(tb->bg, ctx);
		if (ret < 0) {
			exit(127);
		}

		exit(ret);
	} else {
		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		return true;
	}
}

static int task_bg_poll(struct task *task, struct context *ctx) {
	struct task_bg *tb = (struct task_bg *)task;

	if (!tb->started) {
		if (!task_bg_start(task, ctx)) {
			return TASK_STATUS_ERROR;
		}
		tb->started = true;
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
