#ifndef _MRSH_AST_H
#define _MRSH_AST_H

#include <mrsh/array.h>
#include <stdbool.h>

struct mrsh_io_redirect {
	int io_number;
	char *op;
	char *filename;
};

struct mrsh_assignment {
	char *name, *value;
};

enum mrsh_command_type {
	MRSH_SIMPLE_COMMAND,
	MRSH_BRACE_GROUP,
	MRSH_IF_CLAUSE,
};

struct mrsh_command {
	enum mrsh_command_type type;
};

struct mrsh_simple_command {
	struct mrsh_command command;
	char *name;
	struct mrsh_array arguments; // char *
	struct mrsh_array io_redirects; // struct mrsh_io_redirect *
	struct mrsh_array assignments; // struct mrsh_assignment *
};

struct mrsh_brace_group {
	struct mrsh_command command;
	struct mrsh_array body; // struct mrsh_command_list *
};

struct mrsh_if_clause {
	struct mrsh_command command;
	struct mrsh_array condition; // struct mrsh_command_list *
	struct mrsh_array body; // struct mrsh_command_list *
	struct mrsh_command *else_part; // can be NULL
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
	bool bang;
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
	struct mrsh_array body; // struct mrsh_command_list *
};

void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir);
void mrsh_assignment_destroy(struct mrsh_assignment *assign);
void mrsh_command_destroy(struct mrsh_command *cmd);
void mrsh_node_destroy(struct mrsh_node *node);
void mrsh_command_list_destroy(struct mrsh_command_list *l);
void mrsh_program_destroy(struct mrsh_program *prog);
struct mrsh_simple_command *mrsh_simple_command_create(char *name,
	struct mrsh_array *arguments, struct mrsh_array *io_redirects,
	struct mrsh_array *assignments);
struct mrsh_brace_group *mrsh_brace_group_create(struct mrsh_array *body);
struct mrsh_if_clause *mrsh_if_clause_create(struct mrsh_array *condition,
	struct mrsh_array *body, struct mrsh_command *else_part);
struct mrsh_simple_command *mrsh_command_get_simple_command(
	struct mrsh_command *cmd);
struct mrsh_brace_group *mrsh_command_get_brace_group(struct mrsh_command *cmd);
struct mrsh_if_clause *mrsh_command_get_if_clause(struct mrsh_command *cmd);

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands,
	bool bang);
struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
	struct mrsh_node *left, struct mrsh_node *right);
struct mrsh_pipeline *mrsh_node_get_pipeline(struct mrsh_node *node);
struct mrsh_binop *mrsh_node_get_binop(struct mrsh_node *node);

void mrsh_program_print(struct mrsh_program *prog);

#endif
