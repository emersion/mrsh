#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shell/path.h"
#include "shell/task_command.h"

static ssize_t write_here_document_line(int fd, struct mrsh_word *line,
		ssize_t max_size) {
	char *line_str = mrsh_word_str(line);
	size_t line_len = strlen(line_str);
	size_t write_len = line_len + 1; // line + terminating \n
	if (max_size >= 0 && write_len > (size_t)max_size) {
		free(line_str);
		return 0;
	}

	errno = 0;
	ssize_t n = write(fd, line_str, line_len);
	free(line_str);
	if (n < 0 || (size_t)n != line_len) {
		goto err_write;
	}

	if (write(fd, "\n", sizeof(char)) != 1) {
		goto err_write;
	}

	return write_len;

err_write:
	fprintf(stderr, "write() failed: %s\n",
		errno ? strerror(errno) : "short write");
	return -1;
}

static int create_here_document_fd(struct mrsh_array *lines) {
	int fds[2];
	if (pipe(fds) != 0) {
		fprintf(stderr, "pipe() failed: %s", strerror(errno));
		return -1;
	}

	// We can write at most PIPE_BUF bytes without blocking. If we want to write
	// more, we need to fork and continue writing in another process.
	size_t remaining = PIPE_BUF;
	bool more = false;
	size_t i;
	for (i = 0; i < lines->len; ++i) {
		struct mrsh_word *line = lines->data[i];
		ssize_t n = write_here_document_line(fds[1], line, remaining);
		if (n < 0) {
			close(fds[0]);
			close(fds[1]);
			return -1;
		} else if (n == 0) {
			more = true;
			break;
		}
	}

	if (!more) {
		// We could write everything into the pipe buffer
		close(fds[1]);
		return fds[0];
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork() failed: %s", strerror(errno));
		return -1;
	} else if (pid == 0) {
		for (; i < lines->len; ++i) {
			struct mrsh_word *line = lines->data[i];
			ssize_t n = write_here_document_line(fds[1], line, -1);
			if (n < 0) {
				close(fds[1]);
				exit(EXIT_FAILURE);
			}
		}
		close(fds[1]);
		exit(EXIT_SUCCESS);
	}

	return fds[0];
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

static void populate_env_iterator(const char *key, void *_var, void *_) {
	struct mrsh_variable *var = _var;
	if ((var->attribs & MRSH_VAR_ATTRIB_EXPORT)) {
		setenv(key, var->value, 1);
	}
}

static bool task_process_start(struct task_command *tc, struct context *ctx) {
	struct mrsh_simple_command *sc = tc->sc;
	char **argv = (char **)tc->args.data;
	const char *path = expand_path(ctx->state, argv[0], true);
	if (!path) {
		fprintf(stderr, "%s: not found\n", argv[0]);
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			uint32_t prev_attribs;
			if (mrsh_env_get(ctx->state, assign->name, &prev_attribs)
					&& (prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
				fprintf(stderr, "cannot modify readonly variable %s\n",
						assign->name);
				return false;
			}
			char *value = mrsh_word_str(assign->value);
			setenv(assign->name, value, true);
			free(value);
		}

		mrsh_hashtable_for_each(&ctx->state->variables,
				populate_env_iterator, NULL);

		if (ctx->stdin_fileno >= 0) {
			dup2(ctx->stdin_fileno, STDIN_FILENO);
		}
		if (ctx->stdout_fileno >= 0) {
			dup2(ctx->stdout_fileno, STDOUT_FILENO);
		}

		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];

			// TODO: filename expansions
			char *filename = mrsh_word_str(redir->name);

			int fd, default_redir_fd;
			errno = 0;
			switch (redir->op) {
			case MRSH_IO_LESS: // <
				fd = open(filename, O_CLOEXEC | O_RDONLY);
				default_redir_fd = STDIN_FILENO;
				break;
			case MRSH_IO_GREAT: // >
			case MRSH_IO_CLOBBER: // >|
				fd = open(filename,
					O_CLOEXEC | O_WRONLY | O_CREAT | O_TRUNC, 0644);
				default_redir_fd = STDOUT_FILENO;
				break;
			case MRSH_IO_DGREAT: // >>
				fd = open(filename,
					O_CLOEXEC | O_WRONLY | O_CREAT | O_APPEND, 0644);
				default_redir_fd = STDOUT_FILENO;
				break;
			case MRSH_IO_LESSAND: // <&
				// TODO: parse "-"
				fd = parse_fd(filename);
				default_redir_fd = STDIN_FILENO;
				break;
			case MRSH_IO_GREATAND: // >&
				// TODO: parse "-"
				fd = parse_fd(filename);
				default_redir_fd = STDOUT_FILENO;
				break;
			case MRSH_IO_LESSGREAT: // <>
				fd = open(filename, O_CLOEXEC | O_RDWR | O_CREAT, 0644);
				default_redir_fd = STDIN_FILENO;
				break;
			case MRSH_IO_DLESS: // <<
			case MRSH_IO_DLESSDASH: // <<-
				fd = create_here_document_fd(&redir->here_document);
				default_redir_fd = STDIN_FILENO;
				break;
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

			if (fd != redir_fd) {
				close(fd);
			}
		}

		errno = 0;
		execv(path, argv);

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

		process_init(&tc->process, pid);
		return true;
	}
}

int task_process_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;

	if (!tc->started) {
		if (!task_process_start(tc, ctx)) {
			return TASK_STATUS_ERROR;
		}
		tc->started = true;
	}

	return process_poll(&tc->process);
}
