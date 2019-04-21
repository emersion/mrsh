#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/process.h"
#include "shell/task.h"

struct task_async {
	struct task task;
	struct task *async;
	bool started;
};

static void task_async_destroy(struct task *task) {
	struct task_async *ta = (struct task_async *)task;
	task_destroy(ta->async);
	free(ta);
}

static bool task_async_start(struct task *task, struct context *ctx) {
	struct task_async *ta = (struct task_async *)task;

	// Start a subshell
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		if (!(ctx->state->options & MRSH_OPT_MONITOR)) {
			// If job control is disabled, stdin is /dev/null
			int fd = open("/dev/null", O_CLOEXEC | O_RDONLY);
			if (fd < 0) {
				fprintf(stderr, "failed to open /dev/null: %s\n",
					strerror(errno));
				exit(1);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		int ret = task_run(ta->async, ctx);
		if (ret < 0) {
			exit(127);
		}

		exit(ret);
	}

	pid_t pgid = pid;
	if (setpgid(pid, pgid) != 0) {
		fprintf(stderr, "setpgid failed: %s\n", strerror(errno));
		return false;
	}

	struct process *proc = process_create(ctx->state, pid);
	struct mrsh_job *job = job_create(ctx->state, pgid);
	job_add_process(job, proc);

	return true;
}

static int task_async_poll(struct task *task, struct context *ctx) {
	struct task_async *ta = (struct task_async *)task;

	if (!ta->started) {
		if (!task_async_start(task, ctx)) {
			return TASK_STATUS_ERROR;
		}
		ta->started = true;
	}

	return 0;
}

static const struct task_interface task_async_impl = {
	.destroy = task_async_destroy,
	.poll = task_async_poll,
};

struct task *task_async_create(struct task *async) {
	struct task_async *ta = calloc(1, sizeof(struct task_async));
	task_init(&ta->task, &task_async_impl);
	ta->async = async;
	return &ta->task;
}
