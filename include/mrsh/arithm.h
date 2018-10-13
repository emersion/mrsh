#ifndef _MRSH_AST_ARITHM_H
#define _MRSH_AST_ARITHM_H

enum mrsh_arithm_expr_type {
	MRSH_ARITHM_LITERAL,
	MRSH_ARITHM_UNOP,
	MRSH_ARITHM_BINOP,
	MRSH_ARITHM_COND,
	MRSH_ARITHM_ASSIGN,
};

/**
 * An aritmetic expression. One of:
 * - A literal
 * - An unary operation
 * - A binary operation
 * - A condition
 * - An assignment
 */
struct mrsh_arithm_expr {
	enum mrsh_arithm_expr_type type;
};

struct mrsh_arithm_literal {
	struct mrsh_arithm_expr expr;
	long value;
};

enum mrsh_arithm_unop_type {
	MRSH_ARITHM_UNOP_PLUS,
	MRSH_ARITHM_UNOP_MINUS,
	MRSH_ARITHM_UNOP_TILDE,
	MRSH_ARITHM_UNOP_BANG,
};

struct mrsh_arithm_unop {
	struct mrsh_arithm_expr expr;
	enum mrsh_arithm_unop_type type;
	struct mrsh_arithm_expr *body;
};

enum mrsh_arithm_binop_type {
	MRSH_ARITHM_BINOP_ASTERISK,
	MRSH_ARITHM_BINOP_SLASH,
	MRSH_ARITHM_BINOP_PERCENT,
	MRSH_ARITHM_BINOP_PLUS,
	MRSH_ARITHM_BINOP_MINUS,
	MRSH_ARITHM_BINOP_DLESS,
	MRSH_ARITHM_BINOP_DGREAT,
	MRSH_ARITHM_BINOP_LESS,
	MRSH_ARITHM_BINOP_LESSEQ,
	MRSH_ARITHM_BINOP_GREAT,
	MRSH_ARITHM_BINOP_GREATEQ,
	MRSH_ARITHM_BINOP_DEQ,
	MRSH_ARITHM_BINOP_BANGEQ,
	MRSH_ARITHM_BINOP_AND,
	MRSH_ARITHM_BINOP_CIRC,
	MRSH_ARITHM_BINOP_OR,
	MRSH_ARITHM_BINOP_DAND,
	MRSH_ARITHM_BINOP_DOR,
};

struct mrsh_arithm_binop {
	struct mrsh_arithm_expr expr;
	enum mrsh_arithm_binop_type type;
	struct mrsh_arithm_expr *left, *right;
};

struct mrsh_arithm_cond {
	struct mrsh_arithm_expr expr;
	struct mrsh_arithm_expr *condition, *body, *else_part;
};

enum mrsh_arithm_assign_op {
	MRSH_ARITHM_ASSIGN_NONE,
	MRSH_ARITHM_ASSIGN_ASTERISK,
	MRSH_ARITHM_ASSIGN_SLASH,
	MRSH_ARITHM_ASSIGN_PERCENT,
	MRSH_ARITHM_ASSIGN_PLUS,
	MRSH_ARITHM_ASSIGN_MINUS,
	MRSH_ARITHM_ASSIGN_DLESS,
	MRSH_ARITHM_ASSIGN_DGREAT,
	MRSH_ARITHM_ASSIGN_AND,
	MRSH_ARITHM_ASSIGN_CIRC,
	MRSH_ARITHM_ASSIGN_OR,
};

struct mrsh_arithm_assign {
	struct mrsh_arithm_expr expr;
	enum mrsh_arithm_assign_op op;
	char *name;
	struct mrsh_arithm_expr *value;
};

void mrsh_arithm_expr_destroy(struct mrsh_arithm_expr *expr);
struct mrsh_arithm_literal *mrsh_arithm_literal_create(long value);
struct mrsh_arithm_literal *mrsh_arithm_expr_get_literal(
	const struct mrsh_arithm_expr *expr);

#endif
