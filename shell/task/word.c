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
			perror("read");
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
		perror("pipe");
		return TASK_STATUS_ERROR;
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fds[0]);
		close(fds[1]);
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

static const char *parameter_get_value(struct mrsh_state *state,
		const char *name) {
	static char value[16];
	char *end;
	long lvalue = strtol(name, &end, 10);
	// Special cases
	if (strcmp(name, "@") == 0) {
		// TODO
	} else if (strcmp(name, "*") == 0) {
		// TODO
	} else if (strcmp(name, "#") == 0) {
		sprintf(value, "%d", state->frame->argc - 1);
		return value;
	} else if (strcmp(name, "?") == 0) {
		sprintf(value, "%d", state->last_status);
		return value;
	} else if (strcmp(name, "-") == 0) {
		return state_get_options(state);
	} else if (strcmp(name, "$") == 0) {
		sprintf(value, "%d", (int)getpid());
		return value;
	} else if (strcmp(name, "!") == 0) {
		for (ssize_t i = state->jobs.len - 1; i >= 0; i--) {
			struct mrsh_job *job = state->jobs.data[i];
			if (job->processes.len == 0) {
				continue;
			}
			struct process *process =
				job->processes.data[job->processes.len - 1];
			sprintf(value, "%d", process->pid);
			return value;
		}
		/* Standard is unclear on what to do in this case, mimic dash */
		return "";
	} else if (end[0] == '\0' && end != name) {
		if (lvalue >= state->frame->argc) {
			return NULL;
		}
		return state->frame->argv[lvalue];
	}
	// User-set cases
	return mrsh_env_get(state, name, NULL);
}

static struct mrsh_word *create_word_string(const char *str) {
	struct mrsh_word_string *ws = mrsh_word_string_create(strdup(str), false);
	return &ws->word;
}

static struct mrsh_word *copy_word_or_null(struct mrsh_word *word) {
	if (word != NULL) {
		return mrsh_word_copy(word);
	} else {
		return create_word_string("");
	}
}

static int apply_parameter_op(struct context *ctx,
		struct mrsh_word_parameter *wp, const char *str,
		struct mrsh_word **result) {
	switch (wp->op) {
	case MRSH_PARAM_NONE:
		*result = str != NULL ? create_word_string(str) : NULL;
		return 0;
	case MRSH_PARAM_MINUS: // Use Default Values
		if (str == NULL || (str[0] == '\0' && wp->colon)) {
			*result = copy_word_or_null(wp->arg);
		} else {
			*result = create_word_string(str);
		}
		return 0;
	case MRSH_PARAM_EQUAL: // Assign Default Values
		assert(false); // TODO
	case MRSH_PARAM_QMARK: // Indicate Error if Null or Unset
		assert(false); // TODO
	case MRSH_PARAM_PLUS: // Use Alternative Value
		if (str == NULL || (str[0] == '\0' && wp->colon)) {
			*result = create_word_string("");
		} else {
			*result = copy_word_or_null(wp->arg);
		}
		return 0;
	case MRSH_PARAM_LEADING_HASH: // String Length
		if (str == NULL && (ctx->state->options & MRSH_OPT_NOUNSET)) {
			return 0;
		}
		if (strcmp(wp->name, "*") == 0 || strcmp(wp->name, "@") == 0) {
			fprintf(stderr, "%s: using the string length operator on $%s "
				"is undefined behaviour\n",
				ctx->state->frame->argv[0], wp->name);
			return TASK_STATUS_ERROR;
		}

		int len = 0;
		if (str != NULL) {
			len = strlen(str);
		}
		char len_str[32];
		snprintf(len_str, sizeof(len_str), "%d", len);
		*result = create_word_string(len_str);
		return 0;
	case MRSH_PARAM_PERCENT: // Remove Smallest Suffix Pattern
	case MRSH_PARAM_DPERCENT: // Remove Largest Suffix Pattern
	case MRSH_PARAM_HASH: // Remove Smallest Prefix Pattern
	case MRSH_PARAM_DHASH: // Remove Largest Prefix Pattern
		assert(false); // TODO
	}
	assert(false);
}

int run_word(struct context *ctx, struct mrsh_word **word_ptr) {
	struct mrsh_word *word = *word_ptr;

	int ret;
	switch (word->type) {
	case MRSH_WORD_STRING:;
		return 0;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		const char *value = parameter_get_value(ctx->state, wp->name);
		char lineno[16];
		if (value == NULL && strcmp(wp->name, "LINENO") == 0) {
			struct mrsh_position pos;
			mrsh_word_range(word, &pos, NULL);

			snprintf(lineno, sizeof(lineno), "%d", pos.line);

			value = lineno;
		}
		struct mrsh_word *result = NULL;
		ret = apply_parameter_op(ctx, wp, value, &result);
		if (ret < 0) {
			return ret;
		}
		if (result == NULL) {
			if ((ctx->state->options & MRSH_OPT_NOUNSET)) {
				fprintf(stderr, "%s: %s: unbound variable\n",
						ctx->state->frame->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}
			result = create_word_string("");
		}
		if (result->type != MRSH_WORD_STRING) {
			ret = run_word(ctx, &result);
			if (ret < 0) {
				return ret;
			}
		}
		swap_words(word_ptr, result);
		return 0;
	case MRSH_WORD_COMMAND:
		return run_word_command(ctx, word_ptr);
	case MRSH_WORD_ARITHMETIC:;
		// For arithmetic words, we need to expand the arithmetic expression
		// before parsing and evaluating it
		struct mrsh_word_arithmetic *wa = mrsh_word_get_arithmetic(word);
		ret = run_word(ctx, &wa->body);
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
					ctx->state->frame->argv[0], err_pos.line,
					err_pos.column, err_msg);
			} else {
				fprintf(stderr, "expected an arithmetic expression\n");
			}
			ret = TASK_STATUS_ERROR;
		} else {
			long result;
			if (!mrsh_run_arithm_expr(ctx->state, expr, &result)) {
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
			ret = run_word(ctx, child_ptr);
			if (ret < 0) {
				return ret;
			}
		}
		return 0;
	}
	assert(false);
}
