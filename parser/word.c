#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "buffer.h"
#include "parser.h"

static struct mrsh_word *single_quotes(struct mrsh_parser *state) {
	struct mrsh_position begin = state->pos;

	char c = parser_read_char(state);
	assert(c == '\'');

	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			parser_set_error(state, "single quotes not terminated");
			return NULL;
		}
		if (c == '\'') {
			parser_read_char(state);
			break;
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	buffer_append_char(&buf, '\0');
	char *data = buffer_steal(&buf);
	struct mrsh_word_string *ws = mrsh_word_string_create(data, true);
	ws->range.begin = begin;
	ws->range.end = state->pos;
	return &ws->word;
}

size_t peek_name(struct mrsh_parser *state, bool in_braces) {
	// In the shell command language, a word consisting solely of underscores,
	// digits, and alphabetics from the portable character set. The first
	// character of a name is not a digit.

	if (!symbol(state, TOKEN)) {
		return false;
	}

	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];
		if (c != '_' && !isalnum(c)) {
			break;
		} else if (i == 0 && isdigit(c) && !in_braces) {
			break;
		}

		++i;
	}

	return i;
}

size_t peek_word(struct mrsh_parser *state, char end) {
	if (!symbol(state, TOKEN)) {
		return false;
	}

	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];

		switch (c) {
		case '\0':
		case '\n':
		case ')':
			return i;
		case '$':
		case '`':
		case '\'':
		case '"':
		case '\\': // TODO: allow backslash in words
			return 0;
		}

		if (is_operator_start(c) || isblank(c) || c == end) {
			return i;
		}

		++i;
	}
}

bool token(struct mrsh_parser *state, const char *str,
		struct mrsh_range *range) {
	if (!symbol(state, TOKEN)) {
		return false;
	}

	struct mrsh_position begin = state->pos;

	size_t len = strlen(str);
	assert(len > 0);

	if (len == 1 && !isalpha(str[0])) {
		if (parser_peek_char(state) != str[0]) {
			return false;
		}
		parser_read_char(state);
	} else {
		size_t word_len = peek_word(state, 0);
		if (len != word_len || strncmp(state->buf.data, str, word_len) != 0) {
			return false;
		}
		// assert(isalpha(str[i]));

		parser_read(state, NULL, len);
	}

	if (range != NULL) {
		range->begin = begin;
		range->end = state->pos;
	}

	consume_symbol(state);
	return true;
}

bool expect_token(struct mrsh_parser *state, const char *str,
		struct mrsh_range *range) {
	if (token(state, str, range)) {
		return true;
	}
	char msg[128];
	snprintf(msg, sizeof(msg), "expected '%s'", str);
	parser_set_error(state, msg);
	return false;
}

char *read_token(struct mrsh_parser *state, size_t len,
		struct mrsh_range *range) {
	if (!symbol(state, TOKEN)) {
		return NULL;
	}

	struct mrsh_position begin = state->pos;

	char *tok = malloc(len + 1);
	parser_read(state, tok, len);
	tok[len] = '\0';

	if (range != NULL) {
		range->begin = begin;
		range->end = state->pos;
	}

	consume_symbol(state);

	return tok;
}

static struct mrsh_word *word_list(struct mrsh_parser *state, char end) {
	struct mrsh_array children = {0};

	while (true) {
		if (parser_peek_char(state) == end) {
			break;
		}

		struct mrsh_word *child = word(state, end);
		if (child == NULL) {
			break;
		}
		mrsh_array_add(&children, child);

		struct mrsh_position begin = state->pos;
		struct buffer buf = {0};
		while (true) {
			char c = parser_peek_char(state);
			if (!isblank(c)) {
				break;
			}
			buffer_append_char(&buf, parser_read_char(state));
		}
		if (buf.len == 0) {
			break; // word() ended on a non-blank char, stop here
		}
		buffer_append_char(&buf, '\0');
		struct mrsh_word_string *ws =
			mrsh_word_string_create(buffer_steal(&buf), false);
		ws->range.begin = begin;
		ws->range.end = state->pos;
		mrsh_array_add(&children, &ws->word);
		buffer_finish(&buf);
	}

	if (children.len == 0) {
		return NULL;
	} else if (children.len == 1) {
		struct mrsh_word *child = children.data[0];
		mrsh_array_finish(&children);
		return child;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}

static enum mrsh_word_parameter_op char_to_parameter_op_val(char c) {
	switch (c) {
	case '-':
		return MRSH_PARAM_MINUS;
	case '=':
		return MRSH_PARAM_EQUAL;
	case '?':
		return MRSH_PARAM_QMARK;
	case '+':
		return MRSH_PARAM_PLUS;
	default:
		return MRSH_PARAM_NONE;
	}
}

static bool expect_parameter_op(struct mrsh_parser *state,
		enum mrsh_word_parameter_op *op, bool *colon) {
	char c = parser_read_char(state);

	*colon = c == ':';
	if (*colon) {
		c = parser_read_char(state);
	}

	*op = char_to_parameter_op_val(c);
	if (*op != MRSH_PARAM_NONE) {
		return true;
	}

	// Colon can only be used with value operations
	if (*colon) {
		parser_set_error(state, "expected a parameter operation");
		return false;
	}

	// Substring processing operations
	char c_next = parser_peek_char(state);
	bool is_double = c == c_next;
	switch (c) {
	case '%':
		*op = is_double ? MRSH_PARAM_DPERCENT : MRSH_PARAM_PERCENT;
		break;
	case '#':
		*op = is_double ? MRSH_PARAM_DHASH : MRSH_PARAM_HASH;
		break;
	default:
		parser_set_error(state, "expected a parameter operation");
		return false;
	}

	if (is_double) {
		parser_read_char(state);
	}
	return true;
}

static struct mrsh_word_parameter *expect_parameter_expression(
		struct mrsh_parser *state) {
	struct mrsh_position lbrace_pos = state->pos;

	char c = parser_read_char(state);
	assert(c == '{');

	enum mrsh_word_parameter_op op = MRSH_PARAM_NONE;
	struct mrsh_range op_range = {0};
	if (parser_peek_char(state) == '#') {
		op_range.begin = state->pos;
		parser_read_char(state);
		op_range.end = state->pos;
		op = MRSH_PARAM_LEADING_HASH;
	}

	size_t name_len = peek_name(state, true);
	if (name_len == 0) {
		parser_set_error(state, "expected a parameter");
		return NULL;
	}

	struct mrsh_range name_range;
	char *name = read_token(state, name_len, &name_range);

	bool colon = false;
	struct mrsh_word *arg = NULL;
	if (op == MRSH_PARAM_NONE && parser_peek_char(state) != '}') {
		op_range.begin = state->pos;
		if (!expect_parameter_op(state, &op, &colon)) {
			return NULL;
		}
		op_range.end = state->pos;
		arg = word_list(state, '}');
	}

	struct mrsh_position rbrace_pos = state->pos;
	if (parser_read_char(state) != '}') {
		parser_set_error(state, "expected end of parameter");
		return NULL;
	}

	struct mrsh_word_parameter *wp =
		mrsh_word_parameter_create(name, op, colon, arg);
	wp->name_range = name_range;
	wp->op_range = op_range;
	wp->lbrace_pos = lbrace_pos;
	wp->rbrace_pos = rbrace_pos;
	return wp;
}

static struct mrsh_word_command *expect_word_command(
		struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '(');

	struct mrsh_program *prog = mrsh_parse_program(state);
	if (prog == NULL) {
		if (!mrsh_parser_error(state, NULL)) {
			parser_set_error(state, "expected a program");
		}
		return NULL;
	}

	if (!expect_token(state, ")", NULL)) {
		mrsh_program_destroy(prog);
		return NULL;
	}

	return mrsh_word_command_create(prog, false);
}

// Expect parameter expansion or command substitution
struct mrsh_word *expect_dollar(struct mrsh_parser *state) {
	struct mrsh_position dollar_pos = state->pos;
	char c = parser_read_char(state);
	assert(c == '$');

	struct mrsh_word_parameter *wp;
	switch (parser_peek_char(state)) {
	case '{':; // Parameter expansion in the form `${expression}`
		wp = expect_parameter_expression(state);
		if (wp == NULL) {
			return NULL;
		}
		wp->dollar_pos = dollar_pos;
		return &wp->word;
	// Command substitution in the form `$(command)` or arithmetic expansion in
	// the form `$((expression))`
	case '(':;
		char next[2];
		parser_peek(state, next, sizeof(next));
		if (next[1] == '(') {
			// TODO: implement it
			parser_set_error(state, "arithmetic expansion not yet implemented");
			return NULL;
		} else {
			struct mrsh_word_command *wc = expect_word_command(state);
			if (wc == NULL) {
				return NULL;
			}
			// TODO: store dollar_pos in wc
			return &wc->word;
		}
	default:; // Parameter expansion in the form `$parameter`
		size_t name_len = peek_name(state, false);
		if (name_len == 0) {
			name_len = 1;
		}

		struct mrsh_range name_range;
		char *name = read_token(state, name_len, &name_range);

		wp = mrsh_word_parameter_create(name, MRSH_PARAM_NONE, false, NULL);
		wp->dollar_pos = dollar_pos;
		wp->name_range = name_range;
		return &wp->word;
	}
}

struct mrsh_word *back_quotes(struct mrsh_parser *state) {
	struct mrsh_position begin = state->pos;

	char c = parser_read_char(state);
	assert(c == '`');

	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			parser_set_error(state, "back quotes not terminated");
			return NULL;
		}
		if (c == '`') {
			parser_read_char(state);
			break;
		}
		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(state, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '\\':
				parser_read_char(state);
				c = next[1];
				break;
			}
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	struct mrsh_parser *subparser =
		mrsh_parser_create_from_buffer(buf.data, buf.len);
	if (subparser == NULL) {
		goto error;
	}
	struct mrsh_program *prog = mrsh_parse_program(subparser);
	if (prog == NULL) {
		const char *err_msg = mrsh_parser_error(subparser, NULL);
		if (err_msg != NULL) {
			// TODO: how should we handle subparser error position?
			parser_set_error(state, err_msg);
			goto error;
		}
	}
	mrsh_parser_destroy(subparser);

	buffer_finish(&buf);

	struct mrsh_word_command *wc = mrsh_word_command_create(prog, true);
	wc->range.begin = begin;
	wc->range.end = state->pos;
	return &wc->word;

error:
	mrsh_parser_destroy(subparser);
	buffer_finish(&buf);
	return NULL;
}

/**
 * Append a new string word to `children` with the contents of `buf`, and reset
 * `buf`.
 */
static void push_buffer_word_string(struct mrsh_parser *state,
		struct mrsh_array *children, struct buffer *buf,
		struct mrsh_position *child_begin) {
	if (buf->len == 0) {
		*child_begin = (struct mrsh_position){0};
		return;
	}

	buffer_append_char(buf, '\0');

	char *data = buffer_steal(buf);
	struct mrsh_word_string *ws = mrsh_word_string_create(data, false);
	ws->range.begin = *child_begin;
	ws->range.end = state->pos;
	mrsh_array_add(children, &ws->word);

	*child_begin = (struct mrsh_position){0};
}

static struct mrsh_word *double_quotes(struct mrsh_parser *state) {
	struct mrsh_position lquote_pos = state->pos;

	char c = parser_read_char(state);
	assert(c == '"');

	struct mrsh_array children = {0};
	struct buffer buf = {0};
	struct mrsh_position child_begin = {0};
	struct mrsh_position rquote_pos = {0};
	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = state->pos;
		}

		char c = parser_peek_char(state);
		if (c == '\0') {
			parser_set_error(state, "double quotes not terminated");
			return NULL;
		}
		if (c == '"') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			rquote_pos = state->pos;
			parser_read_char(state);
			break;
		}

		if (c == '$') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(state, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '"':
			case '\\':
				parser_read_char(state);
				c = next[1];
				break;
			}

			if (next[1] == '\n') {
				parser_read(state, NULL, 2 * sizeof(char));
				continue;
			}
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	buffer_finish(&buf);

	struct mrsh_word_list *wl = mrsh_word_list_create(&children, true);
	wl->lquote_pos = lquote_pos;
	wl->rquote_pos = rquote_pos;
	return &wl->word;
}

struct mrsh_word *word(struct mrsh_parser *state, char end) {
	if (!symbol(state, TOKEN)) {
		return NULL;
	}

	if (is_operator_start(parser_peek_char(state))
			|| parser_peek_char(state) == ')'
			|| parser_peek_char(state) == end) {
		return NULL;
	}

	struct mrsh_array children = {0};
	struct buffer buf = {0};
	struct mrsh_position child_begin = {0};

	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = state->pos;
		}

		char c = parser_peek_char(state);
		if (c == '\0' || c == '\n' || c == ')' || c == end) {
			break;
		}

		if (c == '$') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		// Quoting
		if (c == '\'') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = single_quotes(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}
		if (c == '"') {
			push_buffer_word_string(state, &children, &buf, &child_begin);
			struct mrsh_word *t = double_quotes(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Unquoted backslash
			parser_read_char(state);
			c = parser_peek_char(state);
			if (c == '\n') {
				// Continuation line
				parser_read_char(state);
				continue;
			}
		} else if (is_operator_start(c) || isblank(c)) {
			break;
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	push_buffer_word_string(state, &children, &buf, &child_begin);
	buffer_finish(&buf);

	consume_symbol(state);

	if (children.len == 1) {
		struct mrsh_word *word = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return word;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}

struct mrsh_word *mrsh_parse_word(struct mrsh_parser *state) {
	parser_set_error(state, NULL);
	return word_list(state, 0);
}
