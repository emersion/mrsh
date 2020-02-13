#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
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

static bool naive_word_streq(struct mrsh_word *word, const char *str) {
	while (word->type == MRSH_WORD_LIST) {
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->children.len != 1) {
			return false;
		}
		word = wl->children.data[0];
	}
	if (word->type != MRSH_WORD_STRING) {
		return false;
	}
	struct mrsh_word_string *ws = mrsh_word_get_string(word);
	return strcmp(ws->str, "trap") == 0;
}

static bool is_print_traps(struct mrsh_program *program) {
	if (program->body.len != 1) {
		return false;
	}
	struct mrsh_command_list *cl = program->body.data[0];
	if (cl->ampersand || cl->and_or_list->type != MRSH_AND_OR_LIST_PIPELINE) {
		return false;
	}
	struct mrsh_pipeline *pipeline =
		mrsh_and_or_list_get_pipeline(cl->and_or_list);
	if (pipeline->bang || pipeline->commands.len != 1) {
		return false;
	}
	struct mrsh_command *cmd = pipeline->commands.data[0];
	if (cmd->type != MRSH_SIMPLE_COMMAND) {
		return false;
	}
	struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
	if (sc->name == NULL || !naive_word_streq(sc->name, "trap")) {
		return false;
	}
	if (sc->arguments.len == 1) {
		struct mrsh_word *arg = sc->arguments.data[0];
		return naive_word_streq(arg, "--");
	} else {
		return sc->arguments.len == 0;
	}
}

static void swap_words(struct mrsh_word **word_ptr, struct mrsh_word *new_word) {
	mrsh_word_destroy(*word_ptr);
	*word_ptr = new_word;
}

static int run_word_command(struct mrsh_context *ctx, struct mrsh_word **word_ptr) {
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

		init_job_child_process(ctx->state);

		// When a subshell is entered, traps that are not being ignored shall
		// be set to the default actions, except in the case of a command
		// substitution containing only a single trap command, when the traps
		// need not be altered.
		if (!wc->program || !is_print_traps(wc->program)) {
			reset_caught_traps(ctx->state);
		}

		if (wc->program != NULL) {
			mrsh_run_program(ctx->state, wc->program);
		}

		exit(ctx->state->exit >= 0 ? ctx->state->exit : 0);
	}

	struct mrsh_process *process = process_create(ctx->state, pid);

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
	ws->split_fields = true;
	swap_words(word_ptr, &ws->word);
	return job_wait_process(process);
}

static const char *parameter_get_value(struct mrsh_state *state,
		const char *name) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	static char value[16];
	char *end;
	long lvalue = strtol(name, &end, 10);
	// Special cases
	if (strcmp(name, "@") == 0 || strcmp(name, "*") == 0) {
		// These are handled separately, because they evaluate to a word, not
		// a raw string.
		return NULL;
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
		for (ssize_t i = priv->jobs.len - 1; i >= 0; i--) {
			struct mrsh_job *job = priv->jobs.data[i];
			if (job->processes.len == 0) {
				continue;
			}
			struct mrsh_process *process =
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

static struct mrsh_word *expand_positional_params(struct mrsh_state *state,
		bool quote_args) {
	const char *ifs = mrsh_env_get(state, "IFS", NULL);
	char sep[2] = {0};
	if (ifs == NULL) {
		sep[0] = ' ';
	} else {
		sep[0] = ifs[0];
	}

	struct mrsh_array words = {0};
	for (int i = 1; i < state->frame->argc; i++) {
		const char *arg = state->frame->argv[i];
		if (i > 1 && sep[0] != '\0') {
			struct mrsh_word_string *ws =
				mrsh_word_string_create(strdup(sep), false);
			ws->split_fields = true;
			mrsh_array_add(&words, &ws->word);
		}
		struct mrsh_word_string *ws =
			mrsh_word_string_create(strdup(arg), quote_args);
		ws->split_fields = true;
		mrsh_array_add(&words, &ws->word);
	}

	struct mrsh_word_list *wl = mrsh_word_list_create(&words, false);
	return &wl->word;
}

static void mark_word_split_fields(struct mrsh_word *word) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		ws->split_fields = true;
		break;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		for (size_t i = 0; i < wl->children.len; i++) {
			mark_word_split_fields(wl->children.data[i]);
		}
		break;
	default:
		break;
	}
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

static bool is_null_word(const struct mrsh_word *word) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		const struct mrsh_word_string *ws = mrsh_word_get_string(word);
		return ws->str[0] == '\0';
	case MRSH_WORD_LIST:;
		const struct mrsh_word_list *wl = mrsh_word_get_list(word);
		for (size_t i = 0; i < wl->children.len; i++) {
			const struct mrsh_word *child = wl->children.data[i];
			if (!is_null_word(child)) {
				return false;
			}
		}
		return true;
	default:
		assert(false);
	}
}

static int apply_parameter_cond_op(struct mrsh_context *ctx,
		struct mrsh_word_parameter *wp, struct mrsh_word *value,
		struct mrsh_word **result) {
	switch (wp->op) {
	case MRSH_PARAM_NONE:
		*result = value;
		return 0;
	case MRSH_PARAM_MINUS: // Use Default Values
		if (value == NULL || (wp->colon && is_null_word(value))) {
			mrsh_word_destroy(value);
			*result = copy_word_or_null(wp->arg);
		} else {
			*result = value;
		}
		return 0;
	case MRSH_PARAM_EQUAL: // Assign Default Values
		if (value == NULL || (wp->colon && is_null_word(value))) {
			mrsh_word_destroy(value);
			*result = copy_word_or_null(wp->arg);
			int ret = run_word(ctx, result);
			if (ret < 0) {
				return ret;
			}
			char *str = mrsh_word_str(*result);
			mrsh_env_set(ctx->state, wp->name, str, 0);
			free(str);
		} else {
			*result = value;
		}
		return 0;
	case MRSH_PARAM_QMARK: // Indicate Error if Null or Unset
		if (value == NULL || (wp->colon && is_null_word(value))) {
			mrsh_word_destroy(value);
			char *err_msg;
			if (wp->arg != NULL) {
				struct mrsh_word *err_msg_word = mrsh_word_copy(wp->arg);
				int ret = run_word(ctx, &err_msg_word);
				if (ret < 0) {
					return ret;
				}
				err_msg = mrsh_word_str(err_msg_word);
				mrsh_word_destroy(err_msg_word);
			} else {
				err_msg = strdup("parameter not set or null");
			}
			fprintf(stderr, "%s: %s: %s\n", ctx->state->frame->argv[0],
				wp->name, err_msg);
			free(err_msg);
			// TODO: make the shell exit if non-interactive
			return TASK_STATUS_ERROR;
		} else {
			*result = value;
		}
		return 0;
	case MRSH_PARAM_PLUS: // Use Alternative Value
		if (value == NULL || (wp->colon && is_null_word(value))) {
			*result = create_word_string("");
		} else {
			*result = copy_word_or_null(wp->arg);
		}
		mrsh_word_destroy(value);
		return 0;
	default:
		assert(false); // unreachable
	}
}

static char *trim_str(const char *str, const char *cut, bool suffix) {
	size_t len = strlen(str);
	size_t cut_len = strlen(cut);
	if (cut_len <= len) {
		if (!suffix && memcmp(str, cut, cut_len) == 0) {
			return strdup(str + cut_len);
		}
		if (suffix && memcmp(str + len - cut_len, cut, cut_len) == 0) {
			return strndup(str, len - cut_len);
		}
	}
	return strdup(str);
}

static char *trim_pattern(const char *str, const char *pattern, bool suffix,
		bool largest) {
	size_t len = strlen(str);
	ssize_t begin, end, delta;
	if ((!suffix && !largest) || (suffix && largest)) {
		begin = 0;
		end = len;
		delta = 1;
	} else {
		begin = len - 1;
		end = -1;
		delta = -1;
	}

	char *buf = strdup(str);
	for (ssize_t i = begin; i != end; i += delta) {
		char ch = buf[i];
		buf[i] = '\0';

		const char *match, *trimmed;
		if (!suffix) {
			match = buf;
			trimmed = str + i;
		} else {
			match = str + i;
			trimmed = buf;
		}

		if (fnmatch(pattern, match, 0) == 0) {
			char *result = strdup(trimmed);
			free(buf);
			return result;
		}

		buf[i] = ch;
	}
	free(buf);

	return strdup(str);
}

static int apply_parameter_str_op(struct mrsh_context *ctx,
		struct mrsh_word_parameter *wp, const char *str,
		struct mrsh_word **result) {
	switch (wp->op) {
	case MRSH_PARAM_LEADING_HASH: // String Length
		if (str == NULL && (ctx->state->options & MRSH_OPT_NOUNSET)) {
			*result = NULL;
			return 0;
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
		if (str == NULL) {
			*result = NULL;
			return 0;
		} else if (wp->arg == NULL) {
			*result = create_word_string(str);
			return 0;
		}

		bool suffix = wp->op == MRSH_PARAM_PERCENT ||
			wp->op == MRSH_PARAM_DPERCENT;
		bool largest = wp->op == MRSH_PARAM_DPERCENT ||
			wp->op == MRSH_PARAM_DHASH;

		struct mrsh_word *pattern = mrsh_word_copy(wp->arg);
		int ret = run_word(ctx, &pattern);
		if (ret < 0) {
			return ret;
		}

		char *result_str;
		char *pattern_str = word_to_pattern(pattern);
		if (pattern_str == NULL) {
			char *arg = mrsh_word_str(pattern);
			mrsh_word_destroy(pattern);
			result_str = trim_str(str, arg, suffix);
			free(arg);
		} else {
			mrsh_word_destroy(pattern);
			result_str = trim_pattern(str, pattern_str, suffix, largest);
			free(pattern_str);
		}

		struct mrsh_word_string *result_ws =
			mrsh_word_string_create(result_str, false);
		*result = &result_ws->word;
		return 0;
	default:
		assert(false);
	}
}

static int _run_word(struct mrsh_context *ctx, struct mrsh_word **word_ptr,
		bool double_quoted) {
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
		switch (wp->op) {
		case MRSH_PARAM_NONE:
		case MRSH_PARAM_MINUS:
		case MRSH_PARAM_EQUAL:
		case MRSH_PARAM_QMARK:
		case MRSH_PARAM_PLUS:;
			struct mrsh_word *value_word;
			if (strcmp(wp->name, "@") == 0) {
				// $@ expands to quoted fields only if it's inside double quotes
				// TODO: error out if expansion is unspecified
				value_word = expand_positional_params(ctx->state,
					double_quoted);
			} else if (strcmp(wp->name, "*") == 0) {
				value_word = expand_positional_params(ctx->state, false);
			} else if (value != NULL) {
				value_word = create_word_string(value);
			} else {
				value_word = NULL;
			}

			ret = apply_parameter_cond_op(ctx, wp, value_word, &result);
			if (ret < 0) {
				return ret;
			}
			break;
		case MRSH_PARAM_LEADING_HASH:
		case MRSH_PARAM_PERCENT:
		case MRSH_PARAM_DPERCENT:
		case MRSH_PARAM_HASH:
		case MRSH_PARAM_DHASH:
			if (strcmp(wp->name, "@") == 0 || strcmp(wp->name, "*") == 0 ||
					(wp->op != MRSH_PARAM_LEADING_HASH &&
					strcmp(wp->name, "#") == 0)) {
				fprintf(stderr, "%s: using this parameter operator on $%s "
					"is undefined behaviour\n",
					ctx->state->frame->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}

			ret = apply_parameter_str_op(ctx, wp, value, &result);
			if (ret < 0) {
				return ret;
			}
			break;
		}

		if (result == NULL) {
			if ((ctx->state->options & MRSH_OPT_NOUNSET)) {
				fprintf(stderr, "%s: %s: unbound variable\n",
						ctx->state->frame->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}
			result = create_word_string("");
		}
		mark_word_split_fields(result);
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
				ws->split_fields = true;
				swap_words(word_ptr, &ws->word);
				ret = 0;
			}
		}
		mrsh_arithm_expr_destroy(expr);
		mrsh_parser_destroy(parser);
		return ret;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);

		struct mrsh_array at_sign_words = {0};
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word **child_ptr =
				(struct mrsh_word **)&wl->children.data[i];

			bool is_at_sign = false;
			if ((*child_ptr)->type == MRSH_WORD_PARAMETER) {
				struct mrsh_word_parameter *wp =
					mrsh_word_get_parameter(*child_ptr);
				is_at_sign = strcmp(wp->name, "@") == 0;
			}

			ret = _run_word(ctx, child_ptr,
				double_quoted || wl->double_quoted);
			if (ret < 0) {
				return ret;
			}

			if (wl->double_quoted && is_at_sign) {
				// Fucking $@ needs special handling: we need to extract the
				// fields it expands to outside of the double quotes
				mrsh_array_add(&at_sign_words, *child_ptr);
			}
		}

		if (at_sign_words.len > 0) {
			// We need to put $@ expansions outside of the double quotes.
			// Disclaimer: this is a PITA.
			struct mrsh_array quoted = {0};
			struct mrsh_array unquoted = {0};
			size_t at_sign_idx = 0;
			for (size_t i = 0; i < wl->children.len; i++) {
				struct mrsh_word *child = wl->children.data[i];
				wl->children.data[i] = NULL; // steal the child
				if (at_sign_idx >= at_sign_words.len ||
						child != at_sign_words.data[at_sign_idx]) {
					mrsh_array_add(&quoted, child);
					continue;
				}

				if (quoted.len > 0) {
					struct mrsh_word_list *quoted_wl =
						mrsh_word_list_create(&quoted, true);
					mrsh_array_add(&unquoted, &quoted_wl->word);
					// `unquoted` has been stolen by mrsh_word_list_create
					quoted = (struct mrsh_array){0};
				}

				mrsh_array_add(&unquoted, at_sign_words.data[at_sign_idx]);

				at_sign_idx++;
			}
			if (quoted.len > 0) {
				struct mrsh_word_list *quoted_wl =
					mrsh_word_list_create(&quoted, true);
				mrsh_array_add(&unquoted, &quoted_wl->word);
			}

			struct mrsh_word_list *unquoted_wl =
				mrsh_word_list_create(&unquoted, false);
			swap_words(word_ptr, &unquoted_wl->word);
		}
		mrsh_array_finish(&at_sign_words);

		return 0;
	}
	assert(false);
}

int run_word(struct mrsh_context *ctx, struct mrsh_word **word_ptr) {
	return _run_word(ctx, word_ptr, false);
}

int expand_word(struct mrsh_context *ctx, const struct mrsh_word *_word,
		struct mrsh_array *expanded_fields) {
	struct mrsh_word *word = mrsh_word_copy(_word);
	expand_tilde(ctx->state, &word, false);

	int ret = run_word(ctx, &word);
	if (ret < 0) {
		return ret;
	}

	struct mrsh_array fields = {0};
	const char *ifs = mrsh_env_get(ctx->state, "IFS", NULL);
	split_fields(&fields, word, ifs);
	mrsh_word_destroy(word);

	if (ctx->state->options & MRSH_OPT_NOGLOB) {
		get_fields_str(expanded_fields, &fields);
	} else {
		if (!expand_pathnames(expanded_fields, &fields)) {
			return TASK_STATUS_ERROR;
		}
	}

	for (size_t i = 0; i < fields.len; ++i) {
		mrsh_word_destroy(fields.data[i]);
	}
	mrsh_array_finish(&fields);

	return ret;
}
