#include <assert.h>
#include <mrsh/arithm.h>
#include <mrsh/ast.h>
#include <stdlib.h>

void mrsh_arithm_expr_destroy(struct mrsh_arithm_expr *expr) {
	switch (expr->type) {
	case MRSH_ARITHM_LITERAL:;
		struct mrsh_arithm_literal *al = mrsh_arithm_expr_get_literal(expr);
		free(al);
		return;
	case MRSH_ARITHM_UNOP:
	case MRSH_ARITHM_BINOP:
	case MRSH_ARITHM_COND:
	case MRSH_ARITHM_ASSIGN:
		assert(false); // TODO
	}
	assert(false);
}

struct mrsh_arithm_literal *mrsh_arithm_literal_create(long value) {
	struct mrsh_arithm_literal *al =
		calloc(1, sizeof(struct mrsh_arithm_literal));
	if (al == NULL) {
		return NULL;
	}
	al->expr.type = MRSH_ARITHM_LITERAL;
	al->value = value;
	return al;
}

struct mrsh_arithm_literal *mrsh_arithm_expr_get_literal(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_LITERAL);
	return (struct mrsh_arithm_literal *)expr;
}
