#include "parser.h"

struct mrsh_arithm_expr *arithm_expr(struct mrsh_parser *state) {
	struct mrsh_word *w = word(state, ')');
	if (w == NULL) {
		return NULL;
	}

	struct mrsh_arithm_token *at = mrsh_arithm_token_create(w);
	return &at->expr;
}
