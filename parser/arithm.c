#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <mrsh/arithm.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static bool parse_char(struct mrsh_parser *parser, char c) {
	if (parser_peek_char(parser) != c) {
		return false;
	}
	parser_read_char(parser);
	return true;
}

static bool parse_whitespace(struct mrsh_parser *parser) {
	if (!isspace(parser_peek_char(parser))) {
		return false;
	}
	parser_read_char(parser);
	return true;
}

static inline void consume_whitespace(struct mrsh_parser *parser) {
	while (parse_whitespace(parser)) {
		// This space is intentionally left blank
	}
}

static bool expect_char(struct mrsh_parser *parser, char c) {
	if (parse_char(parser, c)) {
		return true;
	}
	char msg[128];
	snprintf(msg, sizeof(msg), "expected '%c'", c);
	parser_set_error(parser, msg);
	return false;
}

static bool parse_str(struct mrsh_parser *parser, const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		parser_peek(parser, NULL, i + 1);

		if (parser->buf.data[i] != str[i]) {
			return false;
		}
	}

	parser_read(parser, NULL, len);
	return true;
}

static int ishexdigit(char c) {
	return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static size_t peek_hex_literal(struct mrsh_parser *parser) {
	size_t i = 2; // We already know '0x' prefix is there

	while (true) {
		parser_peek(parser, NULL, i + 1);

		char c = parser->buf.data[i];
		if (!ishexdigit(c)) {
			break;
		}

		++i;
	}

	return i;
}

static size_t peek_literal(struct mrsh_parser *parser) {
	size_t i = 0;

	parser_peek(parser, NULL, 1);

	char c = parser->buf.data[0];
	if (c == '0') {
		++i;
		parser_peek(parser, NULL, 2);

		c = parser->buf.data[1];
		// TODO: 0b prefix
		if (c == 'x' || c == 'X') {
			return peek_hex_literal(parser);
		}
	}

	while (true) {
		if (!isdigit(c)) {
			break;
		}

		++i;
		parser_peek(parser, NULL, i + 1);

		c = parser->buf.data[i];
	}

	return i;
}

static struct mrsh_arithm_literal *literal(struct mrsh_parser *parser) {
	size_t len = peek_literal(parser);
	if (len == 0) {
		return NULL;
	}

	char *str = strndup(parser->buf.data, len);
	parser_read(parser, NULL, len);

	char *end;
	errno = 0;
	long value = strtol(str, &end, 0);
	if (end[0] != '\0' || errno != 0) {
		free(str);
		parser_set_error(parser, "failed to parse literal");
		return NULL;
	}
	free(str);

	return mrsh_arithm_literal_create(value);
}

static struct mrsh_arithm_variable *variable(struct mrsh_parser *parser) {
	size_t name_len = peek_name(parser, false);
	if (name_len == 0) {
		return NULL;
	}

	char *name = malloc(name_len + 1);
	parser_read(parser, name, name_len);
	name[name_len] = '\0';

	return mrsh_arithm_variable_create(name);
}

static struct mrsh_arithm_expr *arithm_expr(struct mrsh_parser *parser);

static struct mrsh_arithm_unop *unop(struct mrsh_parser *parser) {
	enum mrsh_arithm_unop_type type;
	switch (parser_peek_char(parser)) {
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
	parser_read_char(parser);

	struct mrsh_arithm_expr *body = arithm_expr(parser);
	if (body == NULL) {
		parser_set_error(parser,
			"expected an arithmetic expression after unary operator");
		return NULL;
	}

	return mrsh_arithm_unop_create(type, body);
}

static struct mrsh_arithm_expr *paren(struct mrsh_parser *parser) {
	if (!parse_char(parser, '(')) {
		return NULL;
	}

	consume_whitespace(parser);
	struct mrsh_arithm_expr *expr = arithm_expr(parser);
	// consume_whitespace() is not needed here, since the call to arithm_expr()
	// consumes the trailing whitespace.

	if (!expect_char(parser, ')')) {
		mrsh_arithm_expr_destroy(expr);
		return NULL;
	}

	return expr;
}

static struct mrsh_arithm_expr *term(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *expr = paren(parser);
	if (expr != NULL) {
		return expr;
	}

	struct mrsh_arithm_unop *au = unop(parser);
	if (au != NULL) {
		return &au->expr;
	}

	struct mrsh_arithm_literal *al = literal(parser);
	if (al != NULL) {
		return &al->expr;
	}

	struct mrsh_arithm_variable *av = variable(parser);
	if (av != NULL) {
		return &av->expr;
	}

	return NULL;
}

static struct mrsh_arithm_expr *factor(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *expr = term(parser);
	if (expr == NULL) {
		return NULL;
	}

	/* This loop ensures we parse factors as left-assossiative */
	while (true) {
		consume_whitespace(parser);
		enum mrsh_arithm_binop_type type;
		if (parse_char(parser, '*')) {
			type = MRSH_ARITHM_BINOP_ASTERISK;
		} else if (parse_char(parser, '/')) {
			type = MRSH_ARITHM_BINOP_SLASH;
		} else if (parse_char(parser, '%')) {
			type = MRSH_ARITHM_BINOP_PERCENT;
		} else {
			return expr;
		}
		consume_whitespace(parser);

		/* Instead of calling ourselves recursively, we call term for
		 * left-associativity */
		struct mrsh_arithm_expr *right = term(parser);
		if (right == NULL) {
			parser_set_error(parser, "expected a term after *, / or % operator");
			return NULL;
		}

		struct mrsh_arithm_binop *bo =
			mrsh_arithm_binop_create(type, expr, right);
		expr = &bo->expr;
	}
}

static struct mrsh_arithm_expr *addend(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *expr = factor(parser);
	if (expr == NULL) {
		return NULL;
	}

	/* This loop ensures we parse addends as left-assossiative */
	while (true) {
		// consume_whitespace() is not needed here, since the call to factor()
		// consumes trailing whitespace.
		enum mrsh_arithm_binop_type type;
		if (parse_char(parser, '+')) {
			type = MRSH_ARITHM_BINOP_PLUS;
		} else if (parse_char(parser, '-')) {
			type = MRSH_ARITHM_BINOP_MINUS;
		} else {
			return expr;
		}
		consume_whitespace(parser);

		/* Instead of calling ourselves recursively, we call factor for
		 * left-associativity */
		struct mrsh_arithm_expr *right = factor(parser);
		if (right == NULL) {
			parser_set_error(parser, "expected a factor after + or - operator");
			return NULL;
		}

		struct mrsh_arithm_binop *bo =
			mrsh_arithm_binop_create(type, expr, right);
		expr = &bo->expr;
	}
}

static struct mrsh_arithm_expr *shift(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *left = addend(parser);
	if (left == NULL) {
		return NULL;
	}

	// consume_whitespace() is not needed here, since the call to addend()
	// consumes the trailing whitespace.
	enum mrsh_arithm_binop_type type;
	if (parse_str(parser, "<<")) {
		type = MRSH_ARITHM_BINOP_DLESS;
	} else if (parse_str(parser, ">>")) {
		type = MRSH_ARITHM_BINOP_DGREAT;
	} else {
		return left;
	}
	consume_whitespace(parser);

	struct mrsh_arithm_expr *right = shift(parser);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(parser, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *comp(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *left = shift(parser);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_str(parser, "<=")) {
		type = MRSH_ARITHM_BINOP_LESSEQ;
	} else if (parse_char(parser, '<')) {
		type = MRSH_ARITHM_BINOP_LESS;
	} else if (parse_str(parser, ">=")) {
		type = MRSH_ARITHM_BINOP_GREATEQ;
	} else if (parse_char(parser, '>')) {
		type = MRSH_ARITHM_BINOP_GREAT;
	} else {
		return left;
	}
	consume_whitespace(parser);

	struct mrsh_arithm_expr *right = comp(parser);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(parser, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *equal(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *left = comp(parser);
	if (left == NULL) {
		return NULL;
	}

	enum mrsh_arithm_binop_type type;
	if (parse_str(parser, "==")) {
		type = MRSH_ARITHM_BINOP_DEQ;
	} else if (parse_str(parser, "!=")) {
		type = MRSH_ARITHM_BINOP_BANGEQ;
	} else {
		return left;
	}

	struct mrsh_arithm_expr *right = equal(parser);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(parser, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static bool parse_binop(struct mrsh_parser *parser, const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		parser_peek(parser, NULL, i + 1);

		if (parser->buf.data[i] != str[i]) {
			return false;
		}
	}

	// Make sure we don't parse "&&" as "&"
	parser_peek(parser, NULL, len + 1);
	switch (parser->buf.data[len]) {
	case '|':
	case '&':
		return false;
	}

	parser_read(parser, NULL, len);
	return true;
}

static struct mrsh_arithm_expr *binop(struct mrsh_parser *parser,
		enum mrsh_arithm_binop_type type, const char *str,
		struct mrsh_arithm_expr *(*term)(struct mrsh_parser *parser)) {
	struct mrsh_arithm_expr *left = term(parser);
	if (left == NULL) {
		return NULL;
	}
	if (!parse_binop(parser, str)) {
		return left;
	}
	consume_whitespace(parser);

	struct mrsh_arithm_expr *right = binop(parser, type, str, term);
	if (right == NULL) {
		mrsh_arithm_expr_destroy(left);
		parser_set_error(parser, "expected a term");
		return NULL;
	}

	struct mrsh_arithm_binop *bo = mrsh_arithm_binop_create(type, left, right);
	return &bo->expr;
}

static struct mrsh_arithm_expr *bitwise_and(struct mrsh_parser *parser) {
	return binop(parser, MRSH_ARITHM_BINOP_AND, "&", equal);
}

static struct mrsh_arithm_expr *bitwise_xor(struct mrsh_parser *parser) {
	return binop(parser, MRSH_ARITHM_BINOP_CIRC, "^", bitwise_and);
}

static struct mrsh_arithm_expr *bitwise_or(struct mrsh_parser *parser) {
	return binop(parser, MRSH_ARITHM_BINOP_OR, "|", bitwise_xor);
}

static struct mrsh_arithm_expr *logical_and(struct mrsh_parser *parser) {
	return binop(parser, MRSH_ARITHM_BINOP_DAND, "&&", bitwise_or);
}

static struct mrsh_arithm_expr *logical_or(struct mrsh_parser *parser) {
	return binop(parser, MRSH_ARITHM_BINOP_DOR, "||", logical_and);
}

static struct mrsh_arithm_expr *ternary(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *expr = logical_or(parser);
	if (expr == NULL) {
		return NULL;
	}
	if (!parse_char(parser, '?')) {
		return expr;
	}
	struct mrsh_arithm_expr *condition = expr;

	struct mrsh_arithm_expr *body = ternary(parser);
	if (body == NULL) {
		parser_set_error(parser, "expected a logical or term");
		goto error_body;
	}

	if (!expect_char(parser, ':')) {
		goto error_semi;
	}

	struct mrsh_arithm_expr *else_part = ternary(parser);
	if (else_part == NULL) {
		parser_set_error(parser, "expected an or term");
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

static bool peek_assign_op(struct mrsh_parser *parser, size_t *offset,
		const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; ++i) {
		parser_peek(parser, NULL, *offset + i + 1);

		if (parser->buf.data[*offset + i] != str[i]) {
			return false;
		}
	}

	*offset += len;
	return true;
}

static struct mrsh_arithm_expr *assignment(struct mrsh_parser *parser) {
	size_t name_len = peek_name(parser, false);
	if (name_len == 0) {
		return NULL;
	}

	enum mrsh_arithm_assign_op op;
	size_t offset = name_len;
	if (peek_assign_op(parser, &offset, "=")) {
		op = MRSH_ARITHM_ASSIGN_NONE;
	} else if (peek_assign_op(parser, &offset, "*=")) {
		op = MRSH_ARITHM_ASSIGN_ASTERISK;
	} else if (peek_assign_op(parser, &offset, "/=")) {
		op = MRSH_ARITHM_ASSIGN_SLASH;
	} else if (peek_assign_op(parser, &offset, "%=")) {
		op = MRSH_ARITHM_ASSIGN_PERCENT;
	} else if (peek_assign_op(parser, &offset, "+=")) {
		op = MRSH_ARITHM_ASSIGN_PLUS;
	} else if (peek_assign_op(parser, &offset, "-=")) {
		op = MRSH_ARITHM_ASSIGN_MINUS;
	} else if (peek_assign_op(parser, &offset, "<<=")) {
		op = MRSH_ARITHM_ASSIGN_DLESS;
	} else if (peek_assign_op(parser, &offset, ">>=")) {
		op = MRSH_ARITHM_ASSIGN_DGREAT;
	} else if (peek_assign_op(parser, &offset, "&=")) {
		op = MRSH_ARITHM_ASSIGN_AND;
	} else if (peek_assign_op(parser, &offset, "^=")) {
		op = MRSH_ARITHM_ASSIGN_CIRC;
	} else if (peek_assign_op(parser, &offset, "|=")) {
		op = MRSH_ARITHM_ASSIGN_OR;
	} else {
		return NULL;
	}
	// offset is now the offset till the end of the operator

	char *name = malloc(name_len + 1);
	parser_read(parser, name, name_len);
	name[name_len] = '\0';

	parser_read(parser, NULL, offset - name_len); // operator

	struct mrsh_arithm_expr *value = arithm_expr(parser);
	if (value == NULL) {
		free(name);
		parser_set_error(parser, "expected an assignment value");
		return NULL;
	}

	struct mrsh_arithm_assign *a = mrsh_arithm_assign_create(op, name, value);
	return &a->expr;
}

static struct mrsh_arithm_expr *arithm_expr(struct mrsh_parser *parser) {
	struct mrsh_arithm_expr *expr = assignment(parser);
	if (expr != NULL) {
		return expr;
	}

	return ternary(parser);
}

struct mrsh_arithm_expr *mrsh_parse_arithm_expr(struct mrsh_parser *parser) {
	consume_whitespace(parser);

	struct mrsh_arithm_expr *expr = arithm_expr(parser);
	if (expr == NULL) {
		return NULL;
	}

	if (parser_peek_char(parser) != '\0') {
		parser_set_error(parser,
			"garbage at the end of the arithmetic expression");
		mrsh_arithm_expr_destroy(expr);
		return NULL;
	}

	return expr;
}
