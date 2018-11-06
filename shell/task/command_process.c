#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shell/path.h"
#include "shell/shm.h"
#include "shell/task_command.h"

static int create_here_document_file(struct mrsh_array *lines) {
	int fd = create_anonymous_file();
	if (fd < 0) {
		return fd;
	}

	for (size_t i = 0; i < lines->len; ++i) {
		struct mrsh_word *line = lines->data[i];

		char *line_str = mrsh_word_str(line);
		ssize_t n_written = write(fd, line_str, strlen(line_str));
		free(line_str);
		if (n_written < 0) {
			fprintf(stderr, "write() failed: %s\n", strerror(errno));
			close(fd);
			return -1;
		}

		if (write(fd, "\n", sizeof(char)) < 0) {
			fprintf(stderr, "write() failed: %s\n", strerror(errno));
			close(fd);
			return -1;
		}
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		fprintf(stderr, "lseek() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
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
				fd = open(filename, O_RDONLY);
				default_redir_fd = STDIN_FILENO;
				break;
			case MRSH_IO_GREAT: // >
			case MRSH_IO_CLOBBER: // >|
				fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				default_redir_fd = STDOUT_FILENO;
				break;
			case MRSH_IO_DGREAT: // >>
				fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
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
				fd = open(filename, O_RDWR | O_CREAT, 0644);
				default_redir_fd = STDIN_FILENO;
				break;
			case MRSH_IO_DLESS: // <<
			case MRSH_IO_DLESSDASH: // <<-
				fd = create_here_document_file(&redir->here_document);
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
