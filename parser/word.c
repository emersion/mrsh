#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <mrsh/buffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"

static struct mrsh_word *single_quotes(struct mrsh_parser *parser) {
	struct mrsh_position begin = parser->pos;

	char c = parser_read_char(parser);
	assert(c == '\'');

	struct mrsh_buffer buf = {0};

	while (true) {
		char c = parser_peek_char(parser);
		if (c == '\0') {
			parser_set_error(parser, "single quotes not terminated");
			return NULL;
		}
		if (c == '\'') {
			parser_read_char(parser);
			break;
		}

		if (c == '\n') {
			read_continuation_line(parser);
		} else {
			parser_read_char(parser);
		}

		mrsh_buffer_append_char(&buf, c);
	}

	mrsh_buffer_append_char(&buf, '\0');
	char *data = mrsh_buffer_steal(&buf);
	struct mrsh_word_string *ws = mrsh_word_string_create(data, true);
	ws->range.begin = begin;
	ws->range.end = parser->pos;
	return &ws->word;
}

size_t peek_name(struct mrsh_parser *parser, bool in_braces) {
	// In the shell command language, a word consisting solely of underscores,
	// digits, and alphabetics from the portable character set. The first
	// character of a name is not a digit.

	if (!symbol(parser, TOKEN)) {
		return false;
	}

	size_t i = 0;
	while (true) {
		parser_peek(parser, NULL, i + 1);

		char c = parser->buf.data[i];
		if (c != '_' && !isalnum(c)) {
			break;
		} else if (i == 0 && isdigit(c) && !in_braces) {
			break;
		}

		++i;
	}

	return i;
}

size_t peek_word(struct mrsh_parser *parser, char end) {
	if (!symbol(parser, TOKEN)) {
		return false;
	}

	size_t i = 0;
	while (true) {
		parser_peek(parser, NULL, i + 1);

		char c = parser->buf.data[i];

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

bool token(struct mrsh_parser *parser, const char *str,
		struct mrsh_range *range) {
	if (!symbol(parser, TOKEN)) {
		return false;
	}

	struct mrsh_position begin = parser->pos;

	size_t len = strlen(str);
	assert(len > 0);

	if (len == 1 && !isalpha(str[0])) {
		if (parser_peek_char(parser) != str[0]) {
			return false;
		}
		parser_read_char(parser);
	} else {
		size_t word_len = peek_word(parser, 0);
		if (len != word_len || strncmp(parser->buf.data, str, word_len) != 0) {
			return false;
		}
		// assert(isalpha(str[i]));

		parser_read(parser, NULL, len);
	}

	if (range != NULL) {
		range->begin = begin;
		range->end = parser->pos;
	}

	consume_symbol(parser);
	return true;
}

bool expect_token(struct mrsh_parser *parser, const char *str,
		struct mrsh_range *range) {
	if (token(parser, str, range)) {
		return true;
	}
	char msg[128];
	snprintf(msg, sizeof(msg), "expected '%s'", str);
	parser_set_error(parser, msg);
	return false;
}

char *read_token(struct mrsh_parser *parser, size_t len,
		struct mrsh_range *range) {
	if (!symbol(parser, TOKEN)) {
		return NULL;
	}

	struct mrsh_position begin = parser->pos;

	char *tok = malloc(len + 1);
	parser_read(parser, tok, len);
	tok[len] = '\0';

	if (range != NULL) {
		range->begin = begin;
		range->end = parser->pos;
	}

	consume_symbol(parser);

	return tok;
}

static struct mrsh_word *word_list(struct mrsh_parser *parser, char end,
		word_func f) {
	struct mrsh_array children = {0};

	while (true) {
		if (parser_peek_char(parser) == end) {
			break;
		}

		struct mrsh_word *child = f(parser, end);
		if (child == NULL) {
			break;
		}
		mrsh_array_add(&children, child);

		struct mrsh_position begin = parser->pos;
		struct mrsh_buffer buf = {0};
		while (true) {
			char c = parser_peek_char(parser);
			if (!isblank(c)) {
				break;
			}
			mrsh_buffer_append_char(&buf, parser_read_char(parser));
		}
		if (buf.len == 0) {
			break; // word() ended on a non-blank char, stop here
		}
		mrsh_buffer_append_char(&buf, '\0');
		struct mrsh_word_string *ws =
			mrsh_word_string_create(mrsh_buffer_steal(&buf), false);
		ws->range.begin = begin;
		ws->range.end = parser->pos;
		mrsh_array_add(&children, &ws->word);
		mrsh_buffer_finish(&buf);
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

static bool expect_parameter_op(struct mrsh_parser *parser,
		enum mrsh_word_parameter_op *op, bool *colon) {
	char c = parser_read_char(parser);

	*colon = c == ':';
	if (*colon) {
		c = parser_read_char(parser);
	}

	*op = char_to_parameter_op_val(c);
	if (*op != MRSH_PARAM_NONE) {
		return true;
	}

	// Colon can only be used with value operations
	if (*colon) {
		parser_set_error(parser, "expected a parameter operation");
		return false;
	}

	// Substring processing operations
	char c_next = parser_peek_char(parser);
	bool is_double = c == c_next;
	switch (c) {
	case '%':
		*op = is_double ? MRSH_PARAM_DPERCENT : MRSH_PARAM_PERCENT;
		break;
	case '#':
		*op = is_double ? MRSH_PARAM_DHASH : MRSH_PARAM_HASH;
		break;
	default:
		parser_set_error(parser, "expected a parameter operation");
		return false;
	}

	if (is_double) {
		parser_read_char(parser);
	}
	return true;
}

static struct mrsh_word_parameter *expect_parameter_expression(
		struct mrsh_parser *parser) {
	struct mrsh_position lbrace_pos = parser->pos;

	char c = parser_read_char(parser);
	assert(c == '{');

	enum mrsh_word_parameter_op op = MRSH_PARAM_NONE;
	struct mrsh_range op_range = {0};
	if (parser_peek_char(parser) == '#') {
		op_range.begin = parser->pos;
		parser_read_char(parser);
		op_range.end = parser->pos;
		op = MRSH_PARAM_LEADING_HASH;
	}

	size_t name_len = peek_name(parser, true);
	if (name_len == 0) {
		parser_set_error(parser, "expected a parameter");
		return NULL;
	}

	struct mrsh_range name_range;
	char *name = read_token(parser, name_len, &name_range);

	bool colon = false;
	struct mrsh_word *arg = NULL;
	if (op == MRSH_PARAM_NONE && parser_peek_char(parser) != '}') {
		op_range.begin = parser->pos;
		if (!expect_parameter_op(parser, &op, &colon)) {
			return NULL;
		}
		op_range.end = parser->pos;
		arg = word_list(parser, '}', word);
	}

	struct mrsh_position rbrace_pos = parser->pos;
	if (parser_read_char(parser) != '}') {
		parser_set_error(parser, "expected end of parameter");
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
		struct mrsh_parser *parser) {
	char c = parser_read_char(parser);
	assert(c == '(');
	assert(symbol(parser, TOKEN));
	consume_symbol(parser);

	// Alias substitution is not allowed inside command substitution, see
	// section 2.2.3
	mrsh_parser_alias_func alias = parser->alias;
	parser->alias = NULL;

	struct mrsh_program *prog = mrsh_parse_program(parser);
	parser->alias = alias;
	if (prog == NULL) {
		if (!mrsh_parser_error(parser, NULL)) {
			parser_set_error(parser, "expected a program");
		}
		return NULL;
	}

	if (!expect_token(parser, ")", NULL)) {
		mrsh_program_destroy(prog);
		return NULL;
	}

	return mrsh_word_command_create(prog, false);
}

static struct mrsh_word_arithmetic *expect_word_arithmetic(
		struct mrsh_parser *parser) {
	char c = parser_read_char(parser);
	assert(c == '(');
	c = parser_read_char(parser);
	assert(c == '(');

	struct mrsh_word *body = word_list(parser, 0, arithmetic_word);
	if (body == NULL) {
		if (!mrsh_parser_error(parser, NULL)) {
			parser_set_error(parser, "expected an arithmetic expression");
		}
		return NULL;
	}

	if (!expect_token(parser, ")", NULL)) {
		mrsh_word_destroy(body);
		return NULL;
	}
	if (!expect_token(parser, ")", NULL)) {
		mrsh_word_destroy(body);
		return NULL;
	}

	return mrsh_word_arithmetic_create(body);
}

// Expect parameter expansion or command substitution
struct mrsh_word *expect_dollar(struct mrsh_parser *parser) {
	struct mrsh_position dollar_pos = parser->pos;
	char c = parser_read_char(parser);
	assert(c == '$');

	struct mrsh_word_parameter *wp;
	c = parser_peek_char(parser);
	switch (c) {
	case '{': // Parameter expansion in the form `${expression}`
		wp = expect_parameter_expression(parser);
		if (wp == NULL) {
			return NULL;
		}
		wp->dollar_pos = dollar_pos;
		return &wp->word;
	// Command substitution in the form `$(command)` or arithmetic expansion in
	// the form `$((expression))`
	case '(':;
		char next[2];
		parser_peek(parser, next, sizeof(next));
		if (next[1] == '(') {
			struct mrsh_word_arithmetic *wa = expect_word_arithmetic(parser);
			if (wa == NULL) {
				return NULL;
			}
			// TODO: store dollar_pos in wa
			return &wa->word;
		} else {
			struct mrsh_word_command *wc = expect_word_command(parser);
			if (wc == NULL) {
				return NULL;
			}
			// TODO: store dollar_pos in wc
			return &wc->word;
		}
	default:; // Parameter expansion in the form `$parameter`
		size_t name_len = peek_name(parser, false);
		if (name_len == 0) {
			bool ok = false;
			switch (c) {
			case '@':
			case '*':
			case '#':
			case '?':
			case '-':
			case '$':
			case '!':
				ok = true;
				break;
			default:
				ok = isdigit(c);
			}
			if (ok) {
				name_len = 1;
			} else {
				// 2.6. If an unquoted '$' is followed by a character that is
				// not one of the following […] the result is unspecified.
				parser_set_error(parser, "invalid parameter name");
				return NULL;
			}
		}

		struct mrsh_range name_range;
		char *name = read_token(parser, name_len, &name_range);

		wp = mrsh_word_parameter_create(name, MRSH_PARAM_NONE, false, NULL);
		wp->dollar_pos = dollar_pos;
		wp->name_range = name_range;
		return &wp->word;
	}
}

struct mrsh_word *back_quotes(struct mrsh_parser *parser) {
	struct mrsh_position begin = parser->pos;

	char c = parser_read_char(parser);
	assert(c == '`');

	struct mrsh_buffer buf = {0};

	while (true) {
		char c = parser_peek_char(parser);
		if (c == '\0') {
			parser_set_error(parser, "back quotes not terminated");
			return NULL;
		}
		if (c == '`') {
			parser_read_char(parser);
			break;
		}
		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(parser, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '\\':
				parser_read_char(parser);
				c = next[1];
				break;
			}
		}

		if (c == '\n') {
			read_continuation_line(parser);
		} else {
			parser_read_char(parser);
		}

		mrsh_buffer_append_char(&buf, c);
	}

	struct mrsh_parser *subparser = mrsh_parser_with_data(buf.data, buf.len);
	if (subparser == NULL) {
		goto error;
	}
	struct mrsh_program *prog = mrsh_parse_program(subparser);
	const char *err_msg = mrsh_parser_error(subparser, NULL);
	if (err_msg != NULL) {
		// TODO: how should we handle subparser error position?
		parser_set_error(parser, err_msg);
		mrsh_program_destroy(prog);
		goto error;
	}
	mrsh_parser_destroy(subparser);

	mrsh_buffer_finish(&buf);

	struct mrsh_word_command *wc = mrsh_word_command_create(prog, true);
	wc->range.begin = begin;
	wc->range.end = parser->pos;
	return &wc->word;

error:
	mrsh_parser_destroy(subparser);
	mrsh_buffer_finish(&buf);
	return NULL;
}

/**
 * Append a new string word to `children` with the contents of `buf`, and reset
 * `buf`.
 */
static void push_buffer_word_string(struct mrsh_parser *parser,
		struct mrsh_array *children, struct mrsh_buffer *buf,
		struct mrsh_position *child_begin) {
	if (buf->len == 0) {
		*child_begin = (struct mrsh_position){0};
		return;
	}

	mrsh_buffer_append_char(buf, '\0');

	char *data = mrsh_buffer_steal(buf);
	struct mrsh_word_string *ws = mrsh_word_string_create(data, false);
	ws->range.begin = *child_begin;
	ws->range.end = parser->pos;
	mrsh_array_add(children, &ws->word);

	*child_begin = (struct mrsh_position){0};
}

static struct mrsh_word *double_quotes(struct mrsh_parser *parser) {
	struct mrsh_position lquote_pos = parser->pos;

	char c = parser_read_char(parser);
	assert(c == '"');

	struct mrsh_array children = {0};
	struct mrsh_buffer buf = {0};
	struct mrsh_position child_begin = {0};
	struct mrsh_position rquote_pos = {0};
	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = parser->pos;
		}

		char c = parser_peek_char(parser);
		if (c == '\0') {
			parser_set_error(parser, "double quotes not terminated");
			return NULL;
		}
		if (c == '"') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			rquote_pos = parser->pos;
			parser_read_char(parser);
			break;
		}

		if (c == '$') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(parser, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '"':
			case '\\':
				parser_read_char(parser);
				c = next[1];
				break;
			}

			if (next[1] == '\n') {
				parser_read_char(parser); // read backslash
				read_continuation_line(parser);
				continue;
			}
		}

		parser_read_char(parser);
		mrsh_buffer_append_char(&buf, c);
	}

	mrsh_buffer_finish(&buf);

	struct mrsh_word_list *wl = mrsh_word_list_create(&children, true);
	wl->lquote_pos = lquote_pos;
	wl->rquote_pos = rquote_pos;
	return &wl->word;
}

struct mrsh_word *word(struct mrsh_parser *parser, char end) {
	if (!symbol(parser, TOKEN)) {
		return NULL;
	}

	if (is_operator_start(parser_peek_char(parser))
			|| parser_peek_char(parser) == ')'
			|| parser_peek_char(parser) == end) {
		return NULL;
	}

	struct mrsh_array children = {0};
	struct mrsh_buffer buf = {0};
	struct mrsh_position child_begin = {0};

	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = parser->pos;
		}

		char c = parser_peek_char(parser);
		if (c == '\0' || c == '\n' || c == ')' || c == end) {
			break;
		}

		if (c == '$') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		// Quoting
		if (c == '\'') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = single_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}
		if (c == '"') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = double_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Unquoted backslash
			parser_read_char(parser);
			c = parser_peek_char(parser);
			if (c == '\n') {
				// Continuation line
				read_continuation_line(parser);
				continue;
			}
		} else if (is_operator_start(c) || isblank(c)) {
			break;
		}

		parser_read_char(parser);
		mrsh_buffer_append_char(&buf, c);
	}

	push_buffer_word_string(parser, &children, &buf, &child_begin);
	mrsh_buffer_finish(&buf);

	consume_symbol(parser);

	if (children.len == 1) {
		struct mrsh_word *word = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return word;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}

/* TODO remove end parameter when no *_word function takes it */
struct mrsh_word *arithmetic_word(struct mrsh_parser *parser, char end) {
	char next[3] = {0};
	char c = parser_peek_char(parser);
	if (c == ')') {
		parser_peek(parser, next, sizeof(*next) * 2);
		if (!strcmp(next, "))")) {
			return NULL;
		}
	}

	struct mrsh_array children = {0};
	struct mrsh_buffer buf = {0};
	struct mrsh_position child_begin = {0};
	int nested_parens = 0;

	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = parser->pos;
		}

		parser_peek(parser, next, sizeof(*next) * 2);
		c = next[0];
		if (c == '\0' || c == '\n' || c == ';'
				|| isblank(c)
				|| (strcmp(next, "))") == 0 && nested_parens == 0)) {
			break;
		}

		if (c == '$') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		// Quoting
		if (c == '\'') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = single_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}
		if (c == '"') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = double_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Unquoted backslash
			parser_read_char(parser);
			c = parser_peek_char(parser);
			if (c == '\n') {
				// Continuation line
				read_continuation_line(parser);
				continue;
			}
		}
		if (!strcmp(next, "<<") || !strcmp(next, ">>")) {
			parser_read_char(parser);
			mrsh_buffer_append_char(&buf, c);
		}

		if (c == '(') {
			nested_parens++;
		} else if (c == ')') {
			if (nested_parens == 0) {
				parser_set_error(parser, "unmatched closing parenthesis "
					"in arithmetic expression");
				return NULL;
			}
			nested_parens--;
		}

		parser_read_char(parser);
		mrsh_buffer_append_char(&buf, c);
	}

	push_buffer_word_string(parser, &children, &buf, &child_begin);
	mrsh_buffer_finish(&buf);

	consume_symbol(parser);

	if (children.len == 1) {
		struct mrsh_word *word = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return word;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}

/**
 * Parses a word, only recognizing parameter expansion. Quoting and operators
 * are ignored. */
struct mrsh_word *parameter_expansion_word(struct mrsh_parser *parser) {
	struct mrsh_array children = {0};
	struct mrsh_buffer buf = {0};
	struct mrsh_position child_begin = {0};

	while (true) {
		if (!mrsh_position_valid(&child_begin)) {
			child_begin = parser->pos;
		}

		char c = parser_peek_char(parser);
		if (c == '\0') {
			break;
		}

		if (c == '$') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = expect_dollar(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(parser, &children, &buf, &child_begin);
			struct mrsh_word *t = back_quotes(parser);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Unquoted backslash
			parser_read_char(parser);
			c = parser_peek_char(parser);
			if (c == '\n') {
				// Continuation line
				read_continuation_line(parser);
				continue;
			}
		}

		parser_read_char(parser);
		mrsh_buffer_append_char(&buf, c);
	}

	push_buffer_word_string(parser, &children, &buf, &child_begin);
	mrsh_buffer_finish(&buf);

	consume_symbol(parser);

	if (children.len == 1) {
		struct mrsh_word *word = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return word;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}
