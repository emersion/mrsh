#include <assert.h>
#include <mrsh/shell.h>

static bool run_arithm_binop(struct mrsh_state *state,
		struct mrsh_arithm_binop *binop, long *result) {
	long left, right;
	if (!mrsh_run_arithm_expr(state, binop->left, &left)) {
		return false;
	}
	if (!mrsh_run_arithm_expr(state, binop->right, &right)) {
		return false;
	}
	switch (binop->type) {
	case MRSH_ARITHM_BINOP_ASTERISK:
		*result = left * right;
		return true;
	case MRSH_ARITHM_BINOP_SLASH:
		*result = left / right;
		return true;
	case MRSH_ARITHM_BINOP_PERCENT:
		*result = left % right;
		return true;
	case MRSH_ARITHM_BINOP_PLUS:
		*result = left + right;
		return true;
	case MRSH_ARITHM_BINOP_MINUS:
		*result = left - right;
		return true;
	case MRSH_ARITHM_BINOP_DLESS:
		*result = left << right;
		return true;
	case MRSH_ARITHM_BINOP_DGREAT:
		*result = left >> right;
		return true;
	case MRSH_ARITHM_BINOP_LESS:
		*result = left < right;
		return true;
	case MRSH_ARITHM_BINOP_LESSEQ:
		*result = left <= right;
		return true;
	case MRSH_ARITHM_BINOP_GREAT:
		*result = left > right;
		return true;
	case MRSH_ARITHM_BINOP_GREATEQ:
		*result = left >= right;
		return true;
	case MRSH_ARITHM_BINOP_DEQ:
		*result = left == right;
		return true;
	case MRSH_ARITHM_BINOP_BANGEQ:
		*result = left != right;
		return true;
	case MRSH_ARITHM_BINOP_AND:
		*result = left & right;
		return true;
	case MRSH_ARITHM_BINOP_CIRC:
		*result = left ^ right;
		return true;
	case MRSH_ARITHM_BINOP_OR:
		*result = left | right;
		return true;
	case MRSH_ARITHM_BINOP_DAND:
		*result = left && right;
		return true;
	case MRSH_ARITHM_BINOP_DOR:
		*result = left || right;
		return true;
	}
	assert(false); // Unknown binary arithmetic operation
}

static bool run_arithm_unop(struct mrsh_state *state,
		struct mrsh_arithm_unop *unop, long *result) {
	long val;
	if (!mrsh_run_arithm_expr(state, unop->body, &val)) {
		return false;
	}
	switch (unop->type) {
	case MRSH_ARITHM_UNOP_PLUS:;
		/* no-op */
		return true;
	case MRSH_ARITHM_UNOP_MINUS:;
		*result = -val;
		return true;
	case MRSH_ARITHM_UNOP_TILDE:;
		*result = ~val;
		return true;
	case MRSH_ARITHM_UNOP_BANG:;
		*result = !val;
		return true;
	}
	assert(false); // Unknown unary arithmetic operation
}

static bool run_arithm_cond(struct mrsh_state *state,
		struct mrsh_arithm_cond *cond, long *result) {
	long condition;
	if (!mrsh_run_arithm_expr(state, cond->condition, &condition)) {
		return false;
	}
	if (condition) {
		if (!mrsh_run_arithm_expr(state, cond->body, result)) {
			return false;
		}
	} else {
		if (!mrsh_run_arithm_expr(state, cond->else_part, result)) {
			return false;
		}
	}
	return true;
}

bool mrsh_run_arithm_expr(struct mrsh_state *state,
		struct mrsh_arithm_expr *expr, long *result) {
	switch (expr->type) {
	case MRSH_ARITHM_LITERAL:;
		struct mrsh_arithm_literal *literal =
			(struct mrsh_arithm_literal *)expr;
		*result = literal->value;
		break;
	case MRSH_ARITHM_BINOP:;
		struct mrsh_arithm_binop *binop =
			(struct mrsh_arithm_binop *)expr;
		return run_arithm_binop(state, binop, result);
	case MRSH_ARITHM_UNOP:;
		struct mrsh_arithm_unop *unop =
			(struct mrsh_arithm_unop *)expr;
		return run_arithm_unop(state, unop, result);
	case MRSH_ARITHM_COND:;
		struct mrsh_arithm_cond *cond =
			(struct mrsh_arithm_cond *)expr;
		return run_arithm_cond(state, cond, result);
	default:
		// TODO
		*result = 42;
		break;
	}
	return true;
}
