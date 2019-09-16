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

static bool parse_whitespace(struct mrsh_parser *state) {
	if (!isspace(parser_peek_char(state))) {
		return false;
	}
	parser_read_char(state);
	return true;
}

static inline void consume_whitespace(struct mrsh_parser *state) {
	while (parse_whitespace(state)) {
		// This space is intentionally left blank
	}
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

static bool parse_str(struct mrsh_parser *state, const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		parser_peek(state, NULL, i + 1);

		if (state->buf.data[i] != str[i]) {
			return false;
		}
	}

	parser_read(state, NULL, len);
	return true;
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

static struct mrsh_arithm_expr *arithm_expr(struct mrsh_parser *state);

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

	struct mrsh_arithm_expr *body = arithm_expr(state);
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

	consume_whitespace(state);
	struct mrsh_arithm_expr *expr = arithm_expr(state);
	// consume_whitespace() is not needed here, since the call to arithm_expr()
	// consumes the trailing whitespace.

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
	struct mrsh_arithm_expr *expr = term(state);
	if (expr == NULL) {
		return NULL;
	}

	/* This loop ensures we parse factors as left-assossiative */
	while (true) {
		consume_whitespace(state);
		enum mrsh_arithm_binop_type type;
		if (parse_char(state, '*')) {
			type = MRSH_ARITHM_BINOP_ASTERISK;
		} else if (parse_char(state, '/')) {
			type = MRSH_ARITHM_BINOP_SLASH;
		} else if (parse_char(state, '%')) {
			type = MRSH_ARITHM_BINOP_PERCENT;
		} else {
			return expr;
		}
		consume_whitespace(state);

		/* Instead of calling ourselves recursively, we call term for
		 * left-associativity */
		struct mrsh_arithm_expr *right = term(state);
		if (right == NULL) {
			parser_set_error(state, "expected a term after *, / or % operator");
			return NULL;
		}

		struct mrsh_arithm_binop *bo =
			mrsh_arithm_binop_create(type, expr, right);
		expr = &bo->expr;
	}
}

static struct mrsh_arithm_expr *addend(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *expr = factor(state);
	if (expr == NULL) {
		return NULL;
	}

	/* This loop ensures we parse addends as left-assossiative */
	while (true) {
		// consume_whitespace() is not needed here, since the call to factor()
		// consumes trailing whitespace.
		enum mrsh_arithm_binop_type type;
		if (parse_char(state, '+')) {
			type = MRSH_ARITHM_BINOP_PLUS;
		} else if (parse_char(state, '-')) {
			type = MRSH_ARITHM_BINOP_MINUS;
		} else {
			return expr;
		}
		consume_whitespace(state);

		/* Instead of calling ourselves recursively, we call factor for
		 * left-associativity */
		struct mrsh_arithm_expr *right = factor(state);
		if (right == NULL) {
			parser_set_error(state, "expected a factor after + or - operator");
			return NULL;
		}

		struct mrsh_arithm_binop *bo =
			mrsh_arithm_binop_create(type, expr, right);
		expr = &bo->expr;
	}
}

static struct mrsh_arithm_expr *shift(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *left = addend(state);
	if (left == NULL) {
		return NULL;
	}

	// consume_whitespace() is not needed here, since the call to addend()
	// consumes the trailing whitespace.
	enum mrsh_arithm_binop_type type;
	if (parse_str(state, "<<")) {
		type = MRSH_ARITHM_BINOP_DLESS;
	} else if (parse_str(state, ">>")) {
		type = MRSH_ARITHM_BINOP_DGREAT;
	} else {
		return left;
	}
	consume_whitespace(state);

	struct mrsh_arithm_expr *right = shift(state);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(state, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *comp(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *left = shift(state);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_str(state, "<=")) {
		type = MRSH_ARITHM_BINOP_LESSEQ;
	} else if (parse_char(state, '<')) {
		type = MRSH_ARITHM_BINOP_LESS;
	} else if (parse_str(state, ">=")) {
		type = MRSH_ARITHM_BINOP_GREATEQ;
	} else if (parse_char(state, '>')) {
		type = MRSH_ARITHM_BINOP_GREAT;
	} else {
		return left;
	}
	consume_whitespace(state);

	struct mrsh_arithm_expr *right = comp(state);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(state, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *equal(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *left = comp(state);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_str(state, "==")) {
		type = MRSH_ARITHM_BINOP_DEQ;
	} else if (parse_str(state, "!=")) {
		type = MRSH_ARITHM_BINOP_BANGEQ;
	} else {
		return left;
	}

	struct mrsh_arithm_expr *right = equal(state);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(state, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static bool parse_binop(struct mrsh_parser *state, const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		parser_peek(state, NULL, i + 1);

		if (state->buf.data[i] != str[i]) {
			return false;
		}
	}

	// Make sure we don't parse "&&" as "&"
	parser_peek(state, NULL, len + 1);
	switch (state->buf.data[len]) {
	case '|':
	case '&':
		return false;
	}

	parser_read(state, NULL, len);
	return true;
}

static struct mrsh_arithm_expr *binop(struct mrsh_parser *state,
		enum mrsh_arithm_binop_type type, const char *str,
		struct mrsh_arithm_expr *(*term)(struct mrsh_parser *state)) {
	struct mrsh_arithm_expr *left = term(state);
	if (left == NULL) {
		return NULL;
	}
	if (!parse_binop(state, str)) {
		return left;
	}
	consume_whitespace(state);

	struct mrsh_arithm_expr *right = binop(state, type, str, term);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(state, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *bitwise_and(struct mrsh_parser *state) {
	return binop(state, MRSH_ARITHM_BINOP_AND, "&", equal);
}

static struct mrsh_arithm_expr *bitwise_xor(struct mrsh_parser *state) {
	return binop(state, MRSH_ARITHM_BINOP_CIRC, "^", bitwise_and);
}

static struct mrsh_arithm_expr *bitwise_or(struct mrsh_parser *state) {
	return binop(state, MRSH_ARITHM_BINOP_OR, "|", bitwise_xor);
}

static struct mrsh_arithm_expr *logical_and(struct mrsh_parser *state) {
	return binop(state, MRSH_ARITHM_BINOP_DAND, "&&", bitwise_or);
}

static struct mrsh_arithm_expr *logical_or(struct mrsh_parser *state) {
	return binop(state, MRSH_ARITHM_BINOP_DOR, "||", logical_and);
}

static struct mrsh_arithm_expr *ternary(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *expr = logical_or(state);
	if (expr == NULL) {
		return NULL;
	}
	if (!parse_char(state, '?')) {
		return expr;
	}
	struct mrsh_arithm_expr *condition = expr;

	struct mrsh_arithm_expr *body = ternary(state);
	if (body == NULL) {
		parser_set_error(state, "expected a logical or term");
		goto error_body;
	}

	if (!expect_char(state, ':')) {
		goto error_semi;
	}

	struct mrsh_arithm_expr *else_part = ternary(state);
	if (else_part == NULL) {
		parser_set_error(state, "expected an or term");
		goto error_else_part;
	}

	struct mrsh_arithm_cond *c =
		mrsh_arithm_cond_create(condition, body, else_part);
	return &c->expr;

error_else_part:
error_semi:
	mrsh_arithm_expr_destroy(body);
error_body:
	mrsh_arithm_expr_destroy(condition);
	return NULL;
}

static struct mrsh_arithm_expr *assignment(struct mrsh_parser *state) {
	size_t name_len = peek_name(state, false);
	if (name_len == 0) {
		return NULL;
	}

	parser_peek(state, NULL, name_len + 1);
	if (state->buf.data[name_len] != '=') {
		return NULL;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';

	parser_read(state, NULL, 1); // equal sign

	enum mrsh_arithm_assign_op op = MRSH_ARITHM_ASSIGN_NONE;
	if (parse_char(state, '*')) {
		op = MRSH_ARITHM_ASSIGN_ASTERISK;
	} else if (parse_char(state, '/')) {
		op = MRSH_ARITHM_ASSIGN_SLASH;
	} else if (parse_char(state, '%')) {
		op = MRSH_ARITHM_ASSIGN_PERCENT;
	} else if (parse_char(state, '+')) {
		op = MRSH_ARITHM_ASSIGN_PLUS;
	} else if (parse_char(state, '-')) {
		op = MRSH_ARITHM_ASSIGN_MINUS;
	} else if (parse_str(state, "<<")) {
		op = MRSH_ARITHM_ASSIGN_DLESS;
	} else if (parse_str(state, ">>")) {
		op = MRSH_ARITHM_ASSIGN_DGREAT;
	} else if (parse_char(state, '&')) {
		op = MRSH_ARITHM_ASSIGN_AND;
	} else if (parse_char(state, '^')) {
		op = MRSH_ARITHM_ASSIGN_CIRC;
	} else if (parse_char(state, '|')) {
		op = MRSH_ARITHM_ASSIGN_OR;
	}

	struct mrsh_arithm_expr *value = arithm_expr(state);
	if (value == NULL) {
		free(name);
		parser_set_error(state, "expected an assignment value");
		return NULL;
	}

	struct mrsh_arithm_assign *a = mrsh_arithm_assign_create(op, name, value);
	return &a->expr;
}

static struct mrsh_arithm_expr *arithm_expr(struct mrsh_parser *state) {
	struct mrsh_arithm_expr *expr = assignment(state);
	if (expr != NULL) {
		return expr;
	}

	return ternary(state);
}

struct mrsh_arithm_expr *mrsh_parse_arithm_expr(struct mrsh_parser *state) {
	consume_whitespace(state);

	struct mrsh_arithm_expr *expr = arithm_expr(state);
	if (expr == NULL) {
		return NULL;
	}

	if (parser_peek_char(state) != '\0') {
		parser_set_error(state,
			"garbage at the end of the arithmetic expression");
		mrsh_arithm_expr_destroy(expr);
		return NULL;
	}

	return expr;
}
