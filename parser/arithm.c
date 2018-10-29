#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <mrsh/arithm.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static bool parse_char(struct mrsh_parser *state, char c) {
	if (parser_peek_char(state) != c) {
		return false;
	}
	parser_read_char(state);
	return true;
}

static bool expect_char(struct mrsh_parser *state, char c) {
	if (parse_char(state, c)) {
		return true;
	}
	char msg[128];
	snprintf(msg, sizeof(msg), "expected '%c'", c);
	parser_set_error(state, msg);
	return false;
}

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
	if (end[0] != '\0' || errno != 0) {
		free(str);
		parser_set_error(state, "failed to parse literal");
		return NULL;
	}
	free(str);

	return mrsh_arithm_literal_create(value);
}

static struct mrsh_arithm_unop *unop(struct mrsh_parser *state) {
	enum mrsh_arithm_unop_type type;
	switch (parser_peek_char(state)) {
	case '+':
		type = MRSH_ARITHM_UNOP_PLUS;
		break;
	case '-':
		type = MRSH_ARITHM_UNOP_MINUS;
		break;
	case '~':
		type = MRSH_ARITHM_UNOP_TILDE;
		break;
	case '!':
		type = MRSH_ARITHM_UNOP_BANG;
		break;
	default:
		return NULL;
	}
	parser_read_char(state);

	struct mrsh_arithm_expr *body = mrsh_parse_arithm_expr(state);
	if (body == NULL) {
		parser_set_error(state,
			"expected an arithmetic expression after unary operator");
		return NULL;
	}

	return mrsh_arithm_unop_create(type, body);
}

static struct mrsh_arithm_expr *paren(struct mrsh_parser *state) {
	if (!parse_char(state, '(')) {
		return NULL;
	}

	struct mrsh_arithm_expr *expr = mrsh_parse_arithm_expr(state);

	if (!expect_char(state, ')')) {
		mrsh_arithm_expr_destroy(expr);
		return NULL;
	}

	return expr;
}

static struct mrsh_arithm_expr *term(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *expr = paren(state);
	if (expr != NULL) {
		return expr;
	}

	struct mrsh_arithm_unop *au = unop(state);
	if (au != NULL) {
		return &au->expr;
	}

	struct mrsh_arithm_literal *al = literal(state);
	if (al != NULL) {
		return &al->expr;
	}

	return NULL;
}

static struct mrsh_arithm_expr *factor(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *left = term(state);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_char(state, '*')) {
		type = MRSH_ARITHM_BINOP_ASTERISK;
	} else if (parse_char(state, '-')) {
		type = MRSH_ARITHM_BINOP_SLASH;
	} else if (parse_char(state, '%')) {
		type = MRSH_ARITHM_BINOP_PERCENT;
	} else {
		return left;
	}

	struct mrsh_arithm_expr *right = term(state);
	if (right == NULL) {
		parser_set_error(state, "expected a term after *, / or % operator");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

struct mrsh_arithm_expr *mrsh_parse_arithm_expr(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *left = factor(state);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_char(state, '+')) {
		type = MRSH_ARITHM_BINOP_PLUS;
	} else if (parse_char(state, '-')) {
		type = MRSH_ARITHM_BINOP_MINUS;
	} else {
		return left;
	}

	struct mrsh_arithm_expr *right = factor(state);
	if (right == NULL) {
		parser_set_error(state, "expected a factor after + or - operator");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}
