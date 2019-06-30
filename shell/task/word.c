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
#include "shell/word.h"

#define READ_SIZE 1024

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

static void swap_words(struct mrsh_word **word_ptr, struct mrsh_word *new_word) {
	mrsh_word_destroy(*word_ptr);
	*word_ptr = new_word;
}

static int run_word_command(struct context *ctx, struct mrsh_word **word_ptr) {
	struct mrsh_word_command *wc = mrsh_word_get_command(*word_ptr);

	int fds[2];
	if (pipe(fds) != 0) {
		fprintf(stderr, "pipe() failed: %s\n", strerror(errno));
		return TASK_STATUS_ERROR;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return TASK_STATUS_ERROR;
	} else if (pid == 0) {
		close(fds[0]);

		dup2(fds[1], STDOUT_FILENO);
		close(fds[1]);

		if (wc->program != NULL) {
			mrsh_run_program(ctx->state, wc->program);
		}

		exit(ctx->state->exit >= 0 ? ctx->state->exit : 0);
	}

	struct process *process = process_create(ctx->state, pid);

	close(fds[1]);
	int child_fd = fds[0];

	struct mrsh_buffer buf = {0};
	if (!buffer_read_from(&buf, child_fd)) {
		mrsh_buffer_finish(&buf);
		close(child_fd);
		return TASK_STATUS_ERROR;
	}
	mrsh_buffer_append_char(&buf, '\0');
	close(child_fd);

	// Trim newlines at the end
	ssize_t i = buf.len - 2;
	while (i >= 0 && buf.data[i] == '\n') {
		buf.data[i] = '\0';
		--i;
	}

	struct mrsh_word_string *ws =
		mrsh_word_string_create(mrsh_buffer_steal(&buf), false);
	swap_words(word_ptr, &ws->word);
	return job_wait_process(process);
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
		if (state->jobs.len == 0) {
			/* Standard is unclear on what to do in this case, mimic dash */
			return "";
		}
		struct mrsh_job *job = state->jobs.data[state->jobs.len - 1];
		struct process *process =
			job->processes.data[job->processes.len - 1];
		sprintf(value, "%d", process->pid);
		return value;
	} else if (end[0] == '\0') {
		if (lvalue >= state->args->argc) {
			return NULL;
		}
		return state->args->argv[lvalue];
	}
	// User-set cases
	return mrsh_env_get(state, name, NULL);
}

int run_word(struct context *ctx, struct mrsh_word **word_ptr,
		enum tilde_expansion tilde_expansion) {
	struct mrsh_word *word = *word_ptr;

	int ret;
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (!ws->single_quoted && tilde_expansion != TILDE_EXPANSION_NONE) {
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
		swap_words(word_ptr, &replacement->word);
		return 0;
	case MRSH_WORD_COMMAND:
		return run_word_command(ctx, word_ptr);
	case MRSH_WORD_ARITHMETIC:;
		// For arithmetic words, we need to expand the arithmetic expression
		// before parsing and evaluating it
		struct mrsh_word_arithmetic *wa = mrsh_word_get_arithmetic(word);
		ret = run_word(ctx, &wa->body, TILDE_EXPANSION_NONE);
		if (ret < 0) {
			return ret;
		}

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
				swap_words(word_ptr, &ws->word);
				ret = 0;
			}
		}
		mrsh_arithm_expr_destroy(expr);
		mrsh_parser_destroy(parser);
		return ret;
	case MRSH_WORD_LIST:;
		// For word lists, we just need to expand each word
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word **child_ptr =
				(struct mrsh_word **)&wl->children.data[i];
			if (i > 0 || wl->double_quoted) {
				tilde_expansion = TILDE_EXPANSION_NONE;
			}
			ret = run_word(ctx, child_ptr, tilde_expansion);
			if (ret < 0) {
				return ret;
			}
		}
		return 0;
	}
	assert(false);
}
