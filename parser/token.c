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

static struct mrsh_token *single_quotes(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '\'');

	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "single quotes not terminated\n");
			exit(EXIT_FAILURE);
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
	struct mrsh_token_string *ts = mrsh_token_string_create(data, true);
	return &ts->token;
}

size_t peek_name(struct mrsh_parser *state) {
	// In the shell command language, a word consisting solely of underscores,
	// digits, and alphabetics from the portable character set. The first
	// character of a name is not a digit.

	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];
		if (c != '_' && !isalnum(c)) {
			break;
		} else if (i == 0 && isdigit(c)) {
			break;
		}

		++i;
	}

	return i;
}

size_t peek_token(struct mrsh_parser *state, char end) {
	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];

		switch (c) {
		case '\0':
		case '\n':
			return i;
		case '$':
		case '`':
		case '\'':
		case '"':
		case '\\': // TODO: allow backslash in tokens
			return 0;
		}

		if (is_operator_start(c) || isblank(c) || c == end) {
			return i;
		}

		++i;
	}
}

static struct mrsh_token *token_list(struct mrsh_parser *state, char end) {
	struct mrsh_array children = {0};

	while (true) {
		if (parser_peek_char(state) == end) {
			break;
		}

		struct mrsh_token *child = word(state, end);
		if (child == NULL) {
			break;
		}
		mrsh_array_add(&children, child);

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
		struct mrsh_token_string *ts =
			mrsh_token_string_create(buffer_steal(&buf), false);
		mrsh_array_add(&children, &ts->token);
		buffer_finish(&buf);
	}

	if (children.len == 0) {
		return NULL;
	} else if (children.len == 1) {
		struct mrsh_token *child = children.data[0];
		mrsh_array_finish(&children);
		return child;
	} else {
		struct mrsh_token_list *tl = mrsh_token_list_create(&children, false);
		return &tl->token;
	}
}

static struct mrsh_token *expect_parameter_expression(
		struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '{');

	// TODO: ${#parameter}

	size_t name_len = peek_name(state);
	if (name_len == 0) {
		parser_set_error(state, "expected a parameter");
		return NULL;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';

	char *op = NULL;
	struct mrsh_token *arg = NULL;
	if (parser_peek_char(state) != '}') {
		char next[2];
		parser_peek(state, next, sizeof(next));
		bool two_letter_op = false;
		switch (next[0]) {
		case ':':
			if (strchr("-=?+", next[1]) == NULL) {
				parser_set_error(state, "expected a parameter operation");
				return NULL;
			}
			two_letter_op = true;
			break;
		case '-':
		case '=':
		case '?':
		case '+':
			break;
		case '%':
		case '#':
			two_letter_op = next[1] == next[0];
			break;
		default:
			parser_set_error(state, "expected a parameter operation");
			return NULL;
		}

		size_t op_len = two_letter_op ? 2 : 1;
		op = malloc(op_len + 1);
		parser_read(state, op, op_len);
		op[op_len] = '\0';

		arg = token_list(state, '}');
	}

	if (parser_read_char(state) != '}') {
		parser_set_error(state, "expected end of parameter");
		return NULL;
	}

	struct mrsh_token_parameter *tp =
		mrsh_token_parameter_create(name, op, arg);
	return &tp->token;
}

struct mrsh_token *expect_parameter(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '$');

	if (parser_peek_char(state) == '{') {
		return expect_parameter_expression(state);
	}

	// TODO: ${expression}

	size_t name_len = peek_name(state);
	if (name_len == 0) {
		name_len = 1;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';

	struct mrsh_token_parameter *tp =
		mrsh_token_parameter_create(name, NULL, NULL);
	return &tp->token;
}

struct mrsh_token *back_quotes(struct mrsh_parser *state) {
	// TODO: support nested back-quotes

	char c = parser_read_char(state);
	assert(c == '`');

	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "back quotes not terminated\n");
			exit(EXIT_FAILURE);
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

	buffer_append_char(&buf, '\0');
	char *data = buffer_steal(&buf);
	struct mrsh_token_command *tc = mrsh_token_command_create(data, true);
	return &tc->token;
}

/**
 * Append a new string token to `children` with the contents of `buf`, and reset
 * `buf`.
 */
static void push_buffer_token_string(struct mrsh_array *children,
		struct buffer *buf) {
	if (buf->len == 0) {
		return;
	}

	buffer_append_char(buf, '\0');

	char *data = buffer_steal(buf);
	struct mrsh_token_string *ts = mrsh_token_string_create(data, false);
	mrsh_array_add(children, &ts->token);
}

static struct mrsh_token *double_quotes(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '"');

	struct mrsh_array children = {0};
	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "double quotes not terminated\n");
			exit(EXIT_FAILURE);
		}
		if (c == '"') {
			parser_read_char(state);
			break;
		}

		if (c == '$') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = expect_parameter(state);
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = back_quotes(state);
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

	push_buffer_token_string(&children, &buf);
	buffer_finish(&buf);

	struct mrsh_token_list *tl = mrsh_token_list_create(&children, true);
	return &tl->token;
}

struct mrsh_token *word(struct mrsh_parser *state, char end) {
	if (!symbol(state, TOKEN)) {
		return NULL;
	}

	if (is_operator_start(parser_peek_char(state))
			|| parser_peek_char(state) == end) {
		return NULL;
	}

	struct mrsh_array children = {0};
	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0' || c == '\n' || c == end) {
			break;
		}

		if (c == '$') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = expect_parameter(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = back_quotes(state);
			mrsh_array_add(&children, t);
			continue;
		}

		// Quoting
		if (c == '\'') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = single_quotes(state);
			mrsh_array_add(&children, t);
			continue;
		}
		if (c == '"') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = double_quotes(state);
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

	push_buffer_token_string(&children, &buf);
	buffer_finish(&buf);

	consume_symbol(state);

	if (children.len == 1) {
		struct mrsh_token *token = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return token;
	} else {
		struct mrsh_token_list *tl = mrsh_token_list_create(&children, false);
		return &tl->token;
	}
}

struct mrsh_token *mrsh_parse_token(struct mrsh_parser *state) {
	return token_list(state, 0);
}
