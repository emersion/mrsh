#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "buffer.h"

void mrsh_token_destroy(struct mrsh_token *token) {
	if (token == NULL) {
		return;
	}

	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);
		free(ts->str);
		free(ts);
		return;
	case MRSH_TOKEN_PARAMETER:;
		struct mrsh_token_parameter *tp = mrsh_token_get_parameter(token);
		assert(tp != NULL);
		free(tp->name);
		free(tp->op);
		mrsh_token_destroy(tp->arg);
		free(tp);
		return;
	case MRSH_TOKEN_COMMAND:;
		struct mrsh_token_command *tc = mrsh_token_get_command(token);
		assert(tc != NULL);
		free(tc->command);
		free(tc);
		return;
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			mrsh_token_destroy(child);
		}
		mrsh_array_finish(&tl->children);
		free(tl);
		return;
	}
	assert(false);
}

void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir) {
	if (redir == NULL) {
		return;
	}
	free(redir->op);
	mrsh_token_destroy(redir->name);
	for (size_t i = 0; i < redir->here_document.len; ++i) {
		char *line = redir->here_document.data[i];
		free(line);
	}
	mrsh_array_finish(&redir->here_document);
	free(redir);
}

void mrsh_assignment_destroy(struct mrsh_assignment *assign) {
	if (assign == NULL) {
		return;
	}
	free(assign->name);
	mrsh_token_destroy(assign->value);
	free(assign);
}

void command_list_array_finish(struct mrsh_array *cmds) {
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
		mrsh_token_destroy(sc->name);
		for (size_t i = 0; i < sc->arguments.len; ++i) {
			struct mrsh_token *arg = sc->arguments.data[i];
			mrsh_token_destroy(arg);
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
		command_list_array_finish(&bg->body);
		free(bg);
		return;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		command_list_array_finish(&ic->condition);
		command_list_array_finish(&ic->body);
		mrsh_command_destroy(ic->else_part);
		free(ic);
		return;
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fd =
			mrsh_command_get_function_definition(cmd);
		free(fd->name);
		mrsh_command_destroy(fd->body);
		free(fd);
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

	command_list_array_finish(&prog->body);
	free(prog);
}

struct mrsh_token_string *mrsh_token_string_create(char *str,
		bool single_quoted) {
	struct mrsh_token_string *ts = calloc(1, sizeof(struct mrsh_token_string));
	ts->token.type = MRSH_TOKEN_STRING;
	ts->str = str;
	ts->single_quoted = single_quoted;
	return ts;
}

struct mrsh_token_parameter *mrsh_token_parameter_create(char *name, char *op,
		struct mrsh_token *arg) {
	struct mrsh_token_parameter *tp =
		calloc(1, sizeof(struct mrsh_token_parameter));
	tp->token.type = MRSH_TOKEN_PARAMETER;
	tp->name = name;
	tp->op = op;
	tp->arg = arg;
	return tp;
}

struct mrsh_token_command *mrsh_token_command_create(char *command,
		bool back_quoted) {
	struct mrsh_token_command *tc =
		calloc(1, sizeof(struct mrsh_token_command));
	tc->token.type = MRSH_TOKEN_COMMAND;
	tc->command = command;
	tc->back_quoted = back_quoted;
	return tc;
}

struct mrsh_token_list *mrsh_token_list_create(struct mrsh_array *children,
		bool double_quoted) {
	struct mrsh_token_list *tl = calloc(1, sizeof(struct mrsh_token_list));
	tl->token.type = MRSH_TOKEN_LIST;
	tl->children = *children;
	tl->double_quoted = double_quoted;
	return tl;
}

struct mrsh_token_string *mrsh_token_get_string(struct mrsh_token *token) {
	if (token->type != MRSH_TOKEN_STRING) {
		return NULL;
	}
	return (struct mrsh_token_string *)token;
}

struct mrsh_token_parameter *mrsh_token_get_parameter(struct mrsh_token *token) {
	if (token->type != MRSH_TOKEN_PARAMETER) {
		return NULL;
	}
	return (struct mrsh_token_parameter *)token;
}

struct mrsh_token_command *mrsh_token_get_command(struct mrsh_token *token) {
	if (token->type != MRSH_TOKEN_COMMAND) {
		return NULL;
	}
	return (struct mrsh_token_command *)token;
}

struct mrsh_token_list *mrsh_token_get_list(struct mrsh_token *token) {
	if (token->type != MRSH_TOKEN_LIST) {
		return NULL;
	}
	return (struct mrsh_token_list *)token;
}

struct mrsh_simple_command *mrsh_simple_command_create(struct mrsh_token *name,
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

struct mrsh_function_definition *mrsh_function_definition_create(char *name,
		struct mrsh_command *body) {
	struct mrsh_function_definition *def =
		calloc(1, sizeof(struct mrsh_function_definition));
	def->command.type = MRSH_FUNCTION_DEFINITION;
	def->name = name;
	def->body = body;
	return def;
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

struct mrsh_function_definition *mrsh_command_get_function_definition(
		struct mrsh_command *cmd) {
	if (cmd->type != MRSH_FUNCTION_DEFINITION) {
		return NULL;
	}
	return (struct mrsh_function_definition *)cmd;
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

static void token_str(struct mrsh_token *token, struct buffer *buf) {
	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);
		buffer_append(buf, ts->str, strlen(ts->str));
		return;
	case MRSH_TOKEN_PARAMETER:
	case MRSH_TOKEN_COMMAND:
		assert(false);
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			token_str(child, buf);
		}
		return;
	}
	assert(false);
}

char *mrsh_token_str(struct mrsh_token *token) {
	struct buffer buf = {0};
	token_str(token, &buf);
	buffer_append_char(&buf, '\0');
	return buffer_steal(&buf);
}
