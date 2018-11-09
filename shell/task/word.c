#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/parser.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "shell/process.h"
#include "shell/task.h"

#define READ_SIZE 1024

struct task_word {
	struct task task;
	struct mrsh_word **word_ptr;
	enum tilde_expansion tilde_expansion;

	// only if it's a command
	bool started;
	struct process process;
	int fd;
};

static bool buffer_read_from(struct mrsh_buffer *buf, int fd) {
	while (true) {
		char *dst = mrsh_buffer_reserve(buf, READ_SIZE);

		ssize_t n = read(fd, dst, READ_SIZE);
		if (n < 0 && errno == EINTR) {
			continue;
		} else if (n < 0) {
			fprintf(stderr, "read() failed: %s\n", strerror(errno));
			return false;
		} else if (n == 0) {
			break;
		}

		buf->len += n;
	}

	return true;
}

static void task_word_swap(struct task_word *tw,
		struct mrsh_word *new_word) {
	mrsh_word_destroy(*tw->word_ptr);
	*tw->word_ptr = new_word;
}

static void task_word_destroy(struct task *task) {
	struct task_word *tw = (struct task_word *)task;
	if (tw->started) {
		process_finish(&tw->process);
	}
	free(tw);
}

static bool task_word_command_start(struct task_word *tw,
		struct context *ctx) {
	struct mrsh_word *word = *tw->word_ptr;
	struct mrsh_word_command *wc = mrsh_word_get_command(word);

	int fds[2];
	if (pipe(fds) != 0) {
		fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		close(fds[0]);

		dup2(fds[1], STDOUT_FILENO);
		close(fds[1]);

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
	}

	close(fds[1]);
	process_init(&tw->process, pid);
	tw->fd = fds[0];
	return true;
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
		sprintf(value, "%d", state->args->argc - 1);
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
		if (lvalue >= state->args->argc) {
			return NULL;
		}
		return state->args->argv[lvalue];
	}
	// User-set cases
	return mrsh_env_get(state, name, NULL);
}

static int task_word_poll(struct task *task, struct context *ctx) {
	struct task_word *tw = (struct task_word *)task;
	struct mrsh_word *word = *tw->word_ptr;

	int ret;
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (!ws->single_quoted && tw->tilde_expansion != TILDE_EXPANSION_NONE) {
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
						ctx->state->args->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}
			value = "";
		}
		struct mrsh_word_string *replacement =
			mrsh_word_string_create(strdup(value), false);
		task_word_swap(tw, &replacement->word);
		return 0;
	case MRSH_WORD_COMMAND:
		if (!tw->started) {
			if (!task_word_command_start(tw, ctx)) {
				return TASK_STATUS_ERROR;
			}
			tw->started = true;

			// TODO: reading from the pipe blocks the whole shell

			struct mrsh_buffer buf = {0};
			if (!buffer_read_from(&buf, tw->fd)) {
				mrsh_buffer_finish(&buf);
				close(tw->fd);
				return TASK_STATUS_ERROR;
			}
			mrsh_buffer_append_char(&buf, '\0');

			close(tw->fd);
			tw->fd = -1;

			// Trim newlines at the end
			ssize_t i = buf.len - 1;
			while (i >= 0 && buf.data[i] == '\n') {
				buf.data[i] = '\0';
				--i;
			}

			struct mrsh_word_string *ws =
				mrsh_word_string_create(mrsh_buffer_steal(&buf), false);
			task_word_swap(tw, &ws->word);
		}

		return process_poll(&tw->process);
	case MRSH_WORD_ARITHMETIC:;
		struct mrsh_word_arithmetic *wa = mrsh_word_get_arithmetic(word);
		char *body_str = mrsh_word_str(wa->body);
		struct mrsh_parser *parser =
			mrsh_parser_with_data(body_str, strlen(body_str));
		free(body_str);
		struct mrsh_arithm_expr *expr = mrsh_parse_arithm_expr(parser);
		if (expr == NULL) {
			struct mrsh_position err_pos;
			const char *err_msg = mrsh_parser_error(parser, &err_pos);
			if (err_msg != NULL) {
				// TODO: improve error line/column
				fprintf(stderr, "%s (arithmetic %d:%d): %s\n",
					ctx->state->args->argv[0], err_pos.line,
					err_pos.column, err_msg);
			} else {
				fprintf(stderr, "expected an arithmetic expression\n");
			}
			ret = TASK_STATUS_ERROR;
		} else {
			long result;
			if (!mrsh_run_arithm_expr(expr, &result)) {
				ret = TASK_STATUS_ERROR;
			} else {
				char buf[32];
				snprintf(buf, sizeof(buf), "%ld", result);

				struct mrsh_word_string *ws =
					mrsh_word_string_create(strdup(buf), false);
				task_word_swap(tw, &ws->word);
				ret = EXIT_SUCCESS;
			}
		}
		mrsh_arithm_expr_destroy(expr);
		mrsh_parser_destroy(parser);
		return ret;
	case MRSH_WORD_LIST:
		assert(false);
	}
	assert(false);
}

static const struct task_interface task_word_impl = {
	.destroy = task_word_destroy,
	.poll = task_word_poll,
};

struct task *task_word_create(struct mrsh_word **word_ptr,
		enum tilde_expansion tilde_expansion) {
	struct mrsh_word *word = *word_ptr;

	if (word->type == MRSH_WORD_LIST) {
		// For word lists, we just need to expand each word
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

	struct task_word *tw = calloc(1, sizeof(struct task_word));
	task_init(&tw->task, &task_word_impl);
	tw->word_ptr = word_ptr;
	tw->tilde_expansion = tilde_expansion;
	struct task *task = &tw->task;

	if (word->type == MRSH_WORD_ARITHMETIC) {
		// For arithmetic words, we need to expand the arithmetic expression
		// before parsing and evaluating it
		struct mrsh_word_arithmetic *wa = mrsh_word_get_arithmetic(word);
		struct task *task_list = task_list_create();
		task_list_add(task_list,
			task_word_create(&wa->body, TILDE_EXPANSION_NONE));
		task_list_add(task_list, task);
		return task_list;
	}

	return task;
}
