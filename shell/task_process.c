#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "shell.h"

struct task_process {
	struct task task;
	struct mrsh_simple_command *sc;
	bool started;
	struct process process;
};

static void task_process_destroy(struct task *task) {
	struct task_process *tp = (struct task_process *)task;
	process_finish(&tp->process);
	free(tp);
}

static int parse_fd(const char *str) {
	char *endptr;
	errno = 0;
	int fd = strtol(str, &endptr, 10);
	if (errno != 0) {
		return -1;
	}
	if (endptr[0] != '\0') {
		errno = EINVAL;
		return -1;
	}

	return fd;
}

static bool task_process_start(struct task *task, struct context *ctx) {
	struct task_process *tp = (struct task_process *)task;
	struct mrsh_simple_command *sc = tp->sc;

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		int argc = 1 + sc->arguments.len;
		char *argv[argc + 1];
		argv[0] = mrsh_token_str(sc->name);
		for (size_t i = 0; i < sc->arguments.len; ++i) {
			struct mrsh_token *token = sc->arguments.data[i];
			argv[i + 1] = mrsh_token_str(token);
		}
		argv[argc] = NULL;

		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			char *value = mrsh_token_str(assign->value);
			setenv(assign->name, value, true);
			free(value);
		}

		if (ctx->stdin_fileno >= 0) {
			dup2(ctx->stdin_fileno, STDIN_FILENO);
		}
		if (ctx->stdout_fileno >= 0) {
			dup2(ctx->stdout_fileno, STDOUT_FILENO);
		}

		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];

			// TODO: filename expansions
			char *filename = mrsh_token_str(redir->filename);

			int fd, default_redir_fd;
			errno = 0;
			if (strcmp(redir->op, "<") == 0) {
				fd = open(filename, O_RDONLY);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, ">") == 0 ||
					strcmp(redir->op, ">|") == 0) {
				fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				default_redir_fd = STDOUT_FILENO;
			} else if (strcmp(redir->op, ">>") == 0) {
				fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
				default_redir_fd = STDOUT_FILENO;
			} else if (strcmp(redir->op, "<>") == 0) {
				fd = open(filename, O_RDWR | O_CREAT, 0644);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, "<&") == 0) {
				// TODO: parse "-"
				fd = parse_fd(filename);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, ">&") == 0) {
				// TODO: parse "-"
				fd = parse_fd(filename);
				default_redir_fd = STDOUT_FILENO;
			} else {
				assert(false);
			}
			if (fd < 0) {
				fprintf(stderr, "cannot open %s: %s\n", filename,
					strerror(errno));
				exit(EXIT_FAILURE);
			}

			free(filename);

			int redir_fd = redir->io_number;
			if (redir_fd < 0) {
				redir_fd = default_redir_fd;
			}

			errno = 0;
			int ret = dup2(fd, redir_fd);
			if (ret < 0) {
				fprintf(stderr, "cannot duplicate file descriptor: %s\n",
					strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		errno = 0;
		execvp(argv[0], argv);

		// Something went wrong
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		exit(127);
	} else {
		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		process_init(&tp->process, pid);
		return true;
	}
}

static int task_process_poll(struct task *task, struct context *ctx) {
	struct task_process *tp = (struct task_process *)task;

	if (!tp->started) {
		if (!task_process_start(task, ctx)) {
			return TASK_STATUS_ERROR;
		}
		tp->started = true;
	}

	return process_poll(&tp->process);
}

static const struct task_interface task_process_impl = {
	.destroy = task_process_destroy,
	.poll = task_process_poll,
};

struct task *task_process_create(struct mrsh_simple_command *sc) {
	struct task_process *tp = calloc(1, sizeof(struct task_process));
	task_init(&tp->task, &task_process_impl);
	tp->sc = sc;
	return &tp->task;
}
