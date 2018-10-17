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
	case MRSH_ARITHM_UNOP:;
		struct mrsh_arithm_unop *au = mrsh_arithm_expr_get_unop(expr);
		mrsh_arithm_expr_destroy(au->body);
		free(au);
		return;
	case MRSH_ARITHM_BINOP:;
		struct mrsh_arithm_binop *ab = mrsh_arithm_expr_get_binop(expr);
		mrsh_arithm_expr_destroy(ab->left);
		mrsh_arithm_expr_destroy(ab->right);
		free(ab);
		return;
	case MRSH_ARITHM_COND:;
		struct mrsh_arithm_cond *ac = mrsh_arithm_expr_get_cond(expr);
		mrsh_arithm_expr_destroy(ac->condition);
		mrsh_arithm_expr_destroy(ac->body);
		mrsh_arithm_expr_destroy(ac->else_part);
		free(ac);
		return;
	case MRSH_ARITHM_ASSIGN:;
		struct mrsh_arithm_assign *aa = mrsh_arithm_expr_get_assign(expr);
		free(aa->name);
		mrsh_arithm_expr_destroy(aa->value);
		free(aa);
		return;
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

struct mrsh_arithm_unop *mrsh_arithm_unop_create(
		enum mrsh_arithm_unop_type type, struct mrsh_arithm_expr *body) {
	struct mrsh_arithm_unop *au =
		calloc(1, sizeof(struct mrsh_arithm_unop));
	if (au == NULL) {
		return NULL;
	}
	au->expr.type = MRSH_ARITHM_UNOP;
	au->type = type;
	au->body = body;
	return au;
}

struct mrsh_arithm_binop *mrsh_arithm_binop_create(
		enum mrsh_arithm_binop_type type, struct mrsh_arithm_expr *left,
		struct mrsh_arithm_expr *right) {
	struct mrsh_arithm_binop *ab =
		calloc(1, sizeof(struct mrsh_arithm_binop));
	if (ab == NULL) {
		return NULL;
	}
	ab->expr.type = MRSH_ARITHM_BINOP;
	ab->type = type;
	ab->left = left;
	ab->right = right;
	return ab;
}

struct mrsh_arithm_cond *mrsh_arithm_cond_create(
		struct mrsh_arithm_expr *condition, struct mrsh_arithm_expr *body,
		struct mrsh_arithm_expr *else_part) {
	struct mrsh_arithm_cond *ac =
		calloc(1, sizeof(struct mrsh_arithm_cond));
	if (ac == NULL) {
		return NULL;
	}
	ac->expr.type = MRSH_ARITHM_COND;
	ac->condition = condition;
	ac->body = body;
	ac->else_part = else_part;
	return ac;
}

struct mrsh_arithm_assign *mrsh_arithm_assign_create(
		enum mrsh_arithm_assign_op op, char *name,
		struct mrsh_arithm_expr *value) {
	struct mrsh_arithm_assign *aa =
		calloc(1, sizeof(struct mrsh_arithm_assign));
	if (aa == NULL) {
		return NULL;
	}
	aa->expr.type = MRSH_ARITHM_ASSIGN;
	aa->op = op;
	aa->name = name;
	aa->value = value;
	return aa;
}

struct mrsh_arithm_literal *mrsh_arithm_expr_get_literal(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_LITERAL);
	return (struct mrsh_arithm_literal *)expr;
}

struct mrsh_arithm_unop *mrsh_arithm_expr_get_unop(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_UNOP);
	return (struct mrsh_arithm_unop *)expr;
}

struct mrsh_arithm_binop *mrsh_arithm_expr_get_binop(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_BINOP);
	return (struct mrsh_arithm_binop *)expr;
}

struct mrsh_arithm_cond *mrsh_arithm_expr_get_cond(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_COND);
	return (struct mrsh_arithm_cond *)expr;
}

struct mrsh_arithm_assign *mrsh_arithm_expr_get_assign(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_ASSIGN);
	return (struct mrsh_arithm_assign *)expr;
}
