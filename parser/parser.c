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

// Keep sorted from the longest to the shortest
const struct symbol operators[] = {
	{ DLESSDASH, "<<-" },
	{ AND_IF, "&&" },
	{ OR_IF, "||" },
	{ DSEMI, ";;" },
	{ DLESS, "<<" },
	{ DGREAT, ">>" },
	{ LESSAND, "<&" },
	{ GREATAND, ">&" },
	{ LESSGREAT, "<>" },
	{ CLOBBER, ">|" },
};

const size_t operators_len = sizeof(operators) / sizeof(operators[0]);
const size_t operators_max_str_len = 3;

const char *keywords[] = {
	"if",
	"then",
	"else",
	"elif",
	"fi",
	"do",
	"done",

	"case",
	"esac",
	"while",
	"until",
	"for",

	"{",
	"}",
	"!",

	"in",
};

const size_t keywords_len = sizeof(keywords) / sizeof(keywords[0]);

static struct mrsh_parser *parser_create(void) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));
	state->pos.line = state->pos.column = 1;
	return state;
}

struct mrsh_parser *mrsh_parser_create(FILE *f) {
	struct mrsh_parser *state = parser_create();
	state->f = f;
	return state;
}

struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len) {
	struct mrsh_parser *state = parser_create();
	buffer_append(&state->buf, buf, len);
	buffer_append_char(&state->buf, '\0');
	return state;
}

void mrsh_parser_destroy(struct mrsh_parser *state) {
	if (state == NULL) {
		return;
	}
	buffer_finish(&state->buf);
	free(state->error.msg);
	free(state);
}

size_t parser_peek(struct mrsh_parser *state, char *buf, size_t size) {
	if (state->f != NULL && size > state->buf.len) {
		size_t n_more = size - state->buf.len;
		char *dst = buffer_reserve(&state->buf, n_more);
		size_t n_read = fread(dst, 1, n_more, state->f);
		state->buf.len += n_read;
		if (n_read < n_more) {
			if (feof(state->f)) {
				buffer_append_char(&state->buf, '\0');
				size = state->buf.len;
			} else {
				// TODO: better error handling
				return 0;
			}
		}
	}
	if (state->f == NULL && size > state->buf.len) {
		size = state->buf.len;
	}

	if (buf != NULL) {
		memcpy(buf, state->buf.data, size);
	}
	return size;
}

char parser_peek_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_peek(state, &c, sizeof(char));
	return c;
}

size_t parser_read(struct mrsh_parser *state, char *buf, size_t size) {
	size_t n = parser_peek(state, buf, size);
	if (n > 0) {
		for (size_t i = 0; i < n; ++i) {
			assert(state->buf.data[i] != '\0');
			++state->pos.offset;
			if (state->buf.data[i] == '\n') {
				++state->pos.line;
				state->pos.column = 1;
			} else {
				++state->pos.column;
			}
		}
		memmove(state->buf.data, state->buf.data + n, state->buf.len - n);
		state->buf.len -= n;

		state->continuation_line = false;
	}
	return n;
}

char parser_read_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_read(state, &c, sizeof(char));
	return c;
}

void read_continuation_line(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '\n');
	state->continuation_line = true;
}

bool is_operator_start(char c) {
	switch (c) {
	case '&':
	case '|':
	case ';':
	case '<':
	case '>':
		return true;
	default:
		return false;
	}
}

void parser_set_error(struct mrsh_parser *state, const char *msg) {
	if (msg != NULL) {
		if (state->error.msg != NULL) {
			return;
		}

		state->here_documents.len = 0;
		state->error.pos = state->pos;
		state->error.msg = strdup(msg);
	} else {
		free(state->error.msg);

		state->error.pos = (struct mrsh_position){0};
		state->error.msg = NULL;
	}
}

const char *mrsh_parser_error(struct mrsh_parser *state,
		struct mrsh_position *pos) {
	if (pos != NULL) {
		*pos = state->error.pos;
	}
	return state->error.msg;
}

void parser_begin(struct mrsh_parser *state) {
	parser_set_error(state, NULL);
	state->continuation_line = false;
}

// See section 2.3 Token Recognition
static void next_symbol(struct mrsh_parser *state) {
	state->has_sym = true;

	char c = parser_peek_char(state);

	if (c == '\0') {
		state->sym = EOF_TOKEN;
		return;
	}
	if (c == '\n') {
		state->sym = NEWLINE;
		return;
	}

	if (is_operator_start(c)) {
		for (size_t i = 0; i < operators_len; ++i) {
			const char *str = operators[i].str;

			size_t j;
			for (j = 0; str[j] != '\0'; ++j) {
				size_t n = j + 1;
				size_t n_read = parser_peek(state, NULL, n);
				if (n != n_read || state->buf.data[j] != str[j]) {
					break;
				}
			}

			if (str[j] == '\0') {
				state->sym = operators[i].name;
				return;
			}
		}
	}

	if (isblank(c)) {
		parser_read_char(state);
		next_symbol(state);
		return;
	}

	if (c == '#') {
		while (true) {
			char c = parser_peek_char(state);
			if (c == '\0' || c == '\n') {
				break;
			}
			parser_read_char(state);
		}
		next_symbol(state);
		return;
	}

	state->sym = TOKEN;
}

enum symbol_name get_symbol(struct mrsh_parser *state) {
	if (!state->has_sym) {
		next_symbol(state);
	}
	return state->sym;
}

void consume_symbol(struct mrsh_parser *state) {
	state->has_sym = false;
}

bool symbol(struct mrsh_parser *state, enum symbol_name sym) {
	return get_symbol(state) == sym;
}

bool eof(struct mrsh_parser *state) {
	return symbol(state, EOF_TOKEN);
}

bool newline(struct mrsh_parser *state) {
	if (!symbol(state, NEWLINE)) {
		return false;
	}
	char c = parser_read_char(state);
	assert(c == '\n');
	consume_symbol(state);
	return true;
}

void linebreak(struct mrsh_parser *state) {
	while (newline(state)) {
		// This space is intentionally left blank
	}
}

bool newline_list(struct mrsh_parser *state) {
	if (!newline(state)) {
		return false;
	}

	linebreak(state);
	return true;
}

bool mrsh_parser_eof(struct mrsh_parser *state) {
	return state->has_sym && state->sym == EOF_TOKEN;
}

void mrsh_parser_set_alias(struct mrsh_parser *state,
		mrsh_parser_alias_func_t alias, void *user_data) {
	state->alias = alias;
	state->alias_user_data = user_data;
}

bool mrsh_parser_continuation_line(struct mrsh_parser *state) {
	return state->continuation_line;
}
