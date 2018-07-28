#include <assert.h>
#include <mrsh/ast.h>
#include <stdbool.h>
#include <stdlib.h>

void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir) {
	if (redir == NULL) {
		return;
	}
	free(redir->op);
	free(redir->filename);
	free(redir);
}

void mrsh_assignment_destroy(struct mrsh_assignment *assign) {
	if (assign == NULL) {
		return;
	}
	free(assign->name);
	free(assign->value);
	free(assign);
}

static void mrsh_command_list_array_finish(struct mrsh_array *cmds) {
	for (size_t i = 0; i < cmds->len; ++i) {
		struct mrsh_command_list *l = cmds->data[i];
		mrsh_command_list_destroy(l);
	}
	mrsh_array_finish(cmds);
}

void mrsh_command_destroy(struct mrsh_command *cmd) {
	if (cmd == NULL) {
		return;
	}

	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		free(sc->name);
		for (size_t i = 0; i < sc->arguments.len; ++i) {
			char *arg = sc->arguments.data[i];
			free(arg);
		}
		mrsh_array_finish(&sc->arguments);
		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
			mrsh_io_redirect_destroy(redir);
		}
		mrsh_array_finish(&sc->io_redirects);
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			mrsh_assignment_destroy(assign);
		}
		mrsh_array_finish(&sc->assignments);
		free(sc);
		return;
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		mrsh_command_list_array_finish(&bg->body);
		free(bg);
		return;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		mrsh_command_list_array_finish(&ic->condition);
		mrsh_command_list_array_finish(&ic->body);
		mrsh_command_destroy(ic->else_part);
		free(ic);
		return;
	}
	assert(0);
}

void mrsh_node_destroy(struct mrsh_node *node) {
	if (node == NULL) {
		return;
	}

	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *p = mrsh_node_get_pipeline(node);
		for (size_t i = 0; i < p->commands.len; ++i) {
			struct mrsh_command *cmd = p->commands.data[i];
			mrsh_command_destroy(cmd);
		}
		mrsh_array_finish(&p->commands);
		free(p);
		return;
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		mrsh_node_destroy(binop->left);
		mrsh_node_destroy(binop->right);
		free(binop);
		return;
	}
	assert(0);
}

void mrsh_command_list_destroy(struct mrsh_command_list *l) {
	if (l == NULL) {
		return;
	}

	mrsh_node_destroy(l->node);
	free(l);
}

void mrsh_program_destroy(struct mrsh_program *prog) {
	if (prog == NULL) {
		return;
	}

	mrsh_command_list_array_finish(&prog->body);
	free(prog);
}

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
