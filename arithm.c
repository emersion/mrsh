#include <assert.h>
#include <mrsh/arithm.h>
#include <mrsh/ast.h>
#include <stdlib.h>

void mrsh_arithm_expr_destroy(struct mrsh_arithm_expr *expr) {
	switch (expr->type) {
	case MRSH_ARITHM_TOKEN:;
		struct mrsh_arithm_token *at = mrsh_arithm_expr_get_token(expr);
		mrsh_word_destroy(at->word);
		free(at);
		return;
	case MRSH_ARITHM_UNOP:
	case MRSH_ARITHM_BINOP:
	case MRSH_ARITHM_COND:
	case MRSH_ARITHM_ASSIGN:
		assert(false); // TODO
	}
	assert(false);
}

struct mrsh_arithm_token *mrsh_arithm_token_create(struct mrsh_word *word) {
	struct mrsh_arithm_token *at = calloc(1, sizeof(struct mrsh_arithm_token));
	if (at == NULL) {
		return NULL;
	}
	at->expr.type = MRSH_ARITHM_TOKEN;
	at->word = word;
	return at;
}

struct mrsh_arithm_token *mrsh_arithm_expr_get_token(
		const struct mrsh_arithm_expr *expr) {
	assert(expr->type == MRSH_ARITHM_TOKEN);
	return (struct mrsh_arithm_token *)expr;
}
