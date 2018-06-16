#include <mrsh/ast.h>
#include <stdbool.h>
#include <stdlib.h>

struct mrsh_simple_command *mrsh_simple_command_create(char *name,
		struct mrsh_array *arguments, struct mrsh_array *io_redirects,
		struct mrsh_array *assignments) {
	struct mrsh_simple_command *cmd =
		calloc(1, sizeof(struct mrsh_simple_command));
	cmd->command.type = MRSH_SIMPLE_COMMAND;
	cmd->name = name;
	cmd->arguments = *arguments;
	cmd->io_redirects = *io_redirects;
	cmd->assignments = *assignments;
	return cmd;
}

struct mrsh_brace_group *mrsh_brace_group_create(struct mrsh_array *body) {
	struct mrsh_brace_group *group = calloc(1, sizeof(struct mrsh_brace_group));
	group->command.type = MRSH_BRACE_GROUP;
	group->body = *body;
	return group;
}

struct mrsh_if_clause *mrsh_if_clause_create(struct mrsh_array *condition,
		struct mrsh_array *body, struct mrsh_command *else_part) {
	struct mrsh_if_clause *group = calloc(1, sizeof(struct mrsh_if_clause));
	group->command.type = MRSH_IF_CLAUSE;
	group->condition = *condition;
	group->body = *body;
	group->else_part = else_part;
	return group;
}

struct mrsh_simple_command *mrsh_command_get_simple_command(
		struct mrsh_command *cmd) {
	if (cmd->type != MRSH_SIMPLE_COMMAND) {
		return NULL;
	}
	return (struct mrsh_simple_command *)cmd;
}

struct mrsh_brace_group *mrsh_command_get_brace_group(
		struct mrsh_command *cmd) {
	if (cmd->type != MRSH_BRACE_GROUP) {
		return NULL;
	}
	return (struct mrsh_brace_group *)cmd;
}

struct mrsh_if_clause *mrsh_command_get_if_clause(struct mrsh_command *cmd) {
	if (cmd->type != MRSH_IF_CLAUSE) {
		return NULL;
	}
	return (struct mrsh_if_clause *)cmd;
}

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands,
		bool bang) {
	struct mrsh_pipeline *pl = calloc(1, sizeof(struct mrsh_pipeline));
	pl->node.type = MRSH_NODE_PIPELINE;
	pl->commands = *commands;
	pl->bang = bang;
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
