#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <mrsh/arithm.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static size_t peek_literal(struct mrsh_parser *state) {
	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];
		// TODO: 0x, 0b prefixes
		if (!isdigit(c)) {
			break;
		}

		++i;
	}

	return i;
}

static struct mrsh_arithm_literal *literal(struct mrsh_parser *state) {
	size_t len = peek_literal(state);
	if (len == 0) {
		return NULL;
	}

	char *str = strndup(state->buf.data, len);
	parser_read(state, NULL, len);

	char *end;
	errno = 0;
	long value = strtol(str, &end, 0);
	free(str);
	if (end[0] != '\0' || errno != 0) {
		parser_set_error(state, "failed to parse literal");
		return NULL;
	}

	return mrsh_arithm_literal_create(value);
}

struct mrsh_arithm_expr *mrsh_parse_arithm_expr(struct mrsh_parser *state) {
	struct mrsh_arithm_literal *al = literal(state);
	if (al != NULL) {
		return &al->expr;
	}

	// TODO

	return NULL;
}
