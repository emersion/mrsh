#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/parser.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "builtin.h"
#include "shell/process.h"
#include "shell/shm.h"
#include "shell/task.h"

#define TOKEN_COMMAND_READ_SIZE 128

struct task_word {
	struct task task;
	struct mrsh_word **word_ptr;
	enum tilde_expansion tilde_expansion;

	// only if it's a command
	bool started;
	struct process process;
	int fd;
};

static bool read_full(int fd, char *buf, size_t size) {
	size_t n_read = 0;
	do {
		ssize_t n = read(fd, buf, size - n_read);
		if (n < 0 && errno == EINTR) {
			continue;
		} else if (n < 0) {
			fprintf(stderr, "failed to read(): %s\n", strerror(errno));
			return false;
		}
		n_read += n;
	} while (n_read < size);
	return true;
}

static void task_word_swap(struct task_word *tt,
		struct mrsh_word *new_word) {
	mrsh_word_destroy(*tt->word_ptr);
	*tt->word_ptr = new_word;
}

static bool task_word_command_start(struct task_word *tt,
		struct context *ctx) {
	struct mrsh_word *word = *tt->word_ptr;
	struct mrsh_word_command *wc = mrsh_word_get_command(word);

	int fd = create_anonymous_file();
	if (fd < 0) {
		fprintf(stderr, "failed to create anonymous file: %s\n",
			strerror(errno));
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		dup2(fd, STDOUT_FILENO);

		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		if (wc->program != NULL) {
			mrsh_run_program(ctx->state, wc->program);
		}

		exit(ctx->state->exit >= 0 ? ctx->state->exit : EXIT_SUCCESS);
	} else {
		process_init(&tt->process, pid);
		tt->fd = fd;
		return true;
	}
}

static const char *parameter_get_value(struct mrsh_state *state, char *name) {
	static char value[16];
	char *end;
	long lvalue = strtol(name, &end, 10);
	// Special cases
	if (strcmp(name, "@") == 0) {
		// TODO
	} else if (strcmp(name, "*") == 0) {
		// TODO
	} else if (strcmp(name, "#") == 0) {
		sprintf(value, "%d", state->argc - 1);
		return value;
	} else if (strcmp(name, "?") == 0) {
		sprintf(value, "%d", state->last_status);
		return value;
	} else if (strcmp(name, "-") == 0) {
		return print_options(state);
	} else if (strcmp(name, "$") == 0) {
		sprintf(value, "%d", (int)getpid());
		return value;
	} else if (strcmp(name, "!") == 0) {
		// TODO
	} else if (end[0] == '\0') {
		if (lvalue >= state->argc) {
			return NULL;
		}
		return state->argv[lvalue];
	}
	// User-set cases
	return (const char *)mrsh_env_get(state, name, NULL);
}

static int task_word_poll(struct task *task, struct context *ctx) {
	struct task_word *tt = (struct task_word *)task;
	struct mrsh_word *word = *tt->word_ptr;

	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (!ws->single_quoted && tt->tilde_expansion != TILDE_EXPANSION_NONE) {
			// TODO: TILDE_EXPANSION_ASSIGNMENT
			expand_tilde(ctx->state, &ws->str);
		}
		return 0;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		const char *value = parameter_get_value(ctx->state, wp->name);
		if (value == NULL && strcmp(wp->name, "LINENO") == 0) {
			struct mrsh_position pos;
			mrsh_word_range(word, &pos, NULL);

			char lineno[16];
			snprintf(lineno, sizeof(lineno), "%d", pos.line);

			value = lineno;
		}
		if (value == NULL) {
			if ((ctx->state->options & MRSH_OPT_NOUNSET)) {
				fprintf(stderr, "%s: %s: unbound variable\n",
						ctx->state->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}
			value = "";
		}
		struct mrsh_word_string *replacement =
			mrsh_word_string_create(strdup(value), false);
		task_word_swap(tt, &replacement->word);
		return 0;
	case MRSH_WORD_COMMAND:
		if (!tt->started) {
			if (!task_word_command_start(tt, ctx)) {
				return TASK_STATUS_ERROR;
			}
			tt->started = true;
		}

		int ret = process_poll(&tt->process);
		if (ret != TASK_STATUS_WAIT) {
			off_t size = lseek(tt->fd, 0, SEEK_END);
			if (size < 0) {
				fprintf(stderr, "failed to lseek(): %s\n", strerror(errno));
				return TASK_STATUS_ERROR;
			}
			lseek(tt->fd, 0, SEEK_SET);

			char *buf = malloc(size + 1);
			if (buf == NULL) {
				fprintf(stderr, "failed to malloc(): %s\n", strerror(errno));
				return TASK_STATUS_ERROR;
			}

			if (!read_full(tt->fd, buf, size)) {
				return TASK_STATUS_ERROR;
			}
			buf[size] = '\0';

			close(tt->fd);
			tt->fd = -1;

			// Trim newlines at the end
			ssize_t i = size - 1;
			while (i >= 0 && buf[i] == '\n') {
				buf[i] = '\0';
				--i;
			}

			struct mrsh_word_string *ws = mrsh_word_string_create(buf, false);
			task_word_swap(tt, &ws->word);
		}
		return ret;
	case MRSH_WORD_LIST:
		assert(0);
	}
	assert(0);
}

static const struct task_interface task_word_impl = {
	.poll = task_word_poll,
};

struct task *task_word_create(struct mrsh_word **word_ptr,
		enum tilde_expansion tilde_expansion) {
	struct mrsh_word *word = *word_ptr;

	if (word->type == MRSH_WORD_LIST) {
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		struct task *task_list = task_list_create();
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word **child_ptr =
				(struct mrsh_word **)&wl->children.data[i];
			if (i > 0 || wl->double_quoted) {
				tilde_expansion = TILDE_EXPANSION_NONE;
			}
			task_list_add(task_list,
				task_word_create(child_ptr, tilde_expansion));
		}
		return task_list;
	}

	struct task_word *tt = calloc(1, sizeof(struct task_word));
	task_init(&tt->task, &task_word_impl);
	tt->word_ptr = word_ptr;
	tt->tilde_expansion = tilde_expansion;
	return &tt->task;
}
