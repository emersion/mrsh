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

struct mrsh_parser *mrsh_parser_create(FILE *f) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));
	state->f = f;
	state->lineno = 1;
	return state;
}

struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));

	buffer_append(&state->buf, buf, len);
	buffer_append_char(&state->buf, '\0');

	state->lineno = 1;

	return state;
}

void mrsh_parser_destroy(struct mrsh_parser *state) {
	if (state == NULL) {
		return;
	}
	buffer_finish(&state->buf);
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
			if (state->buf.data[i] == '\n') {
				++state->lineno;
			}
		}
		memmove(state->buf.data, state->buf.data + n, state->buf.len - n);
		state->buf.len -= n;
	}
	return n;
}

char parser_read_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_read(state, &c, sizeof(char));
	return c;
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
	state->here_documents.len = 0;

	fprintf(stderr, "mrsh:%d: syntax error: %s\n", state->lineno, msg);
	exit(EXIT_FAILURE);
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
		char next[OPERATOR_MAX_LEN];
		for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
			const char *str = operators[i].str;
			size_t n = strlen(str);
			size_t n_read = parser_peek(state, next, n);
			if (n_read == n && strncmp(next, str, n) == 0) {
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
