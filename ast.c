#include <stdbool.h>
#include <stdlib.h>

#include "ast.h"

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands) {
	struct mrsh_pipeline *pl = calloc(1, sizeof(struct mrsh_pipeline));
	pl->node.type = MRSH_NODE_PIPELINE;
	pl->commands = *commands;
	return pl;
}

struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
		struct mrsh_node *left, struct mrsh_node *right) {
	struct mrsh_binop *binop = calloc(1, sizeof(struct mrsh_binop));
	binop->node.type = MRSH_NODE_BINOP;
	binop->type = type;
	binop->left = left;
	binop->right = right;
	return binop;
}

struct mrsh_pipeline *mrsh_node_get_pipeline(struct mrsh_node *node) {
	if (node->type != MRSH_NODE_PIPELINE) {
		return NULL;
	}
	return (struct mrsh_pipeline *)node;
}

struct mrsh_binop *mrsh_node_get_binop(struct mrsh_node *node) {
	if (node->type != MRSH_NODE_BINOP) {
		return NULL;
	}
	return (struct mrsh_binop *)node;
}
