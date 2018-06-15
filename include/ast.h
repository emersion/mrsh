#ifndef _MRSH_AST_H
#define _MRSH_AST_H

#include "array.h"

struct mrsh_io_redirect {
	int io_number;
	char *op;
	char *filename;
};

struct mrsh_command {
	char *name;
	struct mrsh_array arguments; // char *
	struct mrsh_array io_redirects; // struct mrsh_io_redirect *
	struct mrsh_array assignments; // char *
};

enum mrsh_node_type {
	MRSH_NODE_PIPELINE,
	MRSH_NODE_BINOP,
};

struct mrsh_node {
	enum mrsh_node_type type;
};

struct mrsh_pipeline {
	struct mrsh_node node;
	struct mrsh_array commands; // struct mrsh_command *
};

enum mrsh_binop_type {
	MRSH_BINOP_AND,
	MRSH_BINOP_OR,
};

struct mrsh_binop {
	struct mrsh_node node;
	enum mrsh_binop_type type;
	struct mrsh_node *left, *right;
};

struct mrsh_command_list {
	struct mrsh_node *node;
	bool ampersand;
};

struct mrsh_program {
	struct mrsh_array commands; // struct mrsh_command_list *
};

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands);
struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
	struct mrsh_node *left, struct mrsh_node *right);
struct mrsh_pipeline *mrsh_node_get_pipeline(struct mrsh_node *node);
struct mrsh_binop *mrsh_node_get_binop(struct mrsh_node *node);

#endif
