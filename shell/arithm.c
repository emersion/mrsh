#include <assert.h>
#include <mrsh/shell.h>
#include <stdlib.h>

static bool run_variable(struct mrsh_state *state, const char *name, long *val,
		uint32_t *attribs) {
	const char *str = mrsh_env_get(state, name, attribs);
	if (str == NULL) {
		if ((state->options & MRSH_OPT_NOUNSET)) {
			fprintf(stderr, "%s: %s: unbound variable\n",
					state->frame->argv[0], name);
			return false;
		}
		*val = 0; // POSIX is not clear what to do in this case
	} else {
		char *end;
		*val = strtod(str, &end);
		if (end == str || end[0] != '\0') {
			fprintf(stderr, "%s: %s: not a number: %s\n",
					state->frame->argv[0], name, str);
			return false;
		}
	}
	return true;
}

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
		if (right == 0) {
			fprintf(stderr, "%s: division by zero: %ld/%ld\n",
				state->frame->argv[0], left, right);
			return false;
		}
		*result = left / right;
		return true;
	case MRSH_ARITHM_BINOP_PERCENT:
		if (right == 0) {
			fprintf(stderr, "%s: division by zero: %ld%%%ld\n",
				state->frame->argv[0], left, right);
			return false;
		}
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

static long run_arithm_assign_op(enum mrsh_arithm_assign_op op,
		long cur, long val, long *result) {
	switch (op) {
	case MRSH_ARITHM_ASSIGN_NONE:
		*result = val;
		return true;
	case MRSH_ARITHM_ASSIGN_ASTERISK:
		*result = cur * val;
		return true;
	case MRSH_ARITHM_ASSIGN_SLASH:
		if (val == 0) {
			fprintf(stderr, "division by zero: %ld/%ld\n", cur, val);
			return false;
		}
		*result = cur / val;
		return true;
	case MRSH_ARITHM_ASSIGN_PERCENT:
		if (val == 0) {
			fprintf(stderr, "division by zero: %ld%%%ld\n", cur, val);
			return false;
		}
		*result = cur % val;
		return true;
	case MRSH_ARITHM_ASSIGN_PLUS:
		*result = cur + val;
		return true;
	case MRSH_ARITHM_ASSIGN_MINUS:
		*result = cur - val;
		return true;
	case MRSH_ARITHM_ASSIGN_DLESS:
		*result = cur << val;
		return true;
	case MRSH_ARITHM_ASSIGN_DGREAT:
		*result = cur >> val;
		return true;
	case MRSH_ARITHM_ASSIGN_AND:
		*result = cur & val;
		return true;
	case MRSH_ARITHM_ASSIGN_CIRC:
		*result = cur ^ val;
		return true;
	case MRSH_ARITHM_ASSIGN_OR:
		*result = cur | val;
		return true;
	}
	assert(false);
}

static bool run_arithm_assign(struct mrsh_state *state,
		struct mrsh_arithm_assign *assign, long *result) {
	long val;
	if (!mrsh_run_arithm_expr(state, assign->value, &val)) {
		return false;
	}
	long cur = 0;
	uint32_t attribs = MRSH_VAR_ATTRIB_NONE;
	if (assign->op != MRSH_ARITHM_ASSIGN_NONE) {
		if (!run_variable(state, assign->name, &cur, &attribs)) {
			return false;
		}
	}
	if (!run_arithm_assign_op(assign->op, cur, val, result)) {
		return false;
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", *result);
	mrsh_env_set(state, assign->name, buf, attribs);

	return true;
}

bool mrsh_run_arithm_expr(struct mrsh_state *state,
		struct mrsh_arithm_expr *expr, long *result) {
	switch (expr->type) {
	case MRSH_ARITHM_LITERAL:;
		struct mrsh_arithm_literal *literal =
			(struct mrsh_arithm_literal *)expr;
		*result = literal->value;
		return true;
	case MRSH_ARITHM_VARIABLE:;
		struct mrsh_arithm_variable *variable =
			(struct mrsh_arithm_variable *)expr;
		return run_variable(state, variable->name, result, NULL);
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
	case MRSH_ARITHM_ASSIGN:;
		struct mrsh_arithm_assign *assign =
			(struct mrsh_arithm_assign *)expr;
		return run_arithm_assign(state, assign, result);
	}
	assert(false);
}
