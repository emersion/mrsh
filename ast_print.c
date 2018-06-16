#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"

#define L_LINE "│ "
#define L_VAL  "├─"
#define L_LAST "└─"
#define L_GAP  "  "

static size_t make_sub_prefix(const char *prefix, bool last, char *buf) {
	if (buf != NULL) {
		memcpy(buf, prefix, strlen(prefix) + 1);
		strcat(buf, last ? L_GAP : L_LINE);
	}
	return strlen(prefix) + strlen(L_LINE) + 1;
}

static void print_prefix(const char *prefix, bool last) {
	printf("%s%s", prefix, last ? L_LAST : L_VAL);
}

static void print_io_redirect(struct mrsh_io_redirect *redir) {
	printf("io_redirect %d %s %s\n", redir->io_number, redir->op, redir->filename);
}

static void print_simple_command(struct mrsh_simple_command *cmd,
		const char *prefix) {
	printf("command %s", cmd->name);
	for (size_t i = 0; i < cmd->arguments.len; ++i) {
		char *arg = cmd->arguments.data[i];
		printf(" %s", arg);
	}
	printf("\n");

	for (size_t i = 0; i < cmd->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = cmd->io_redirects.data[i];
		bool last = i == cmd->io_redirects.len - 1 && cmd->assignments.len == 0;
		print_prefix(prefix, last);
		print_io_redirect(redir);
	}

	for (size_t i = 0; i < cmd->assignments.len; ++i) {
		char *assign = cmd->assignments.data[i];
		bool last = i == cmd->assignments.len - 1;
		print_prefix(prefix, last);
		printf("assignment %s\n", assign);
	}
}

static void print_command_list(struct mrsh_command_list *l, const char *prefix);

static void print_command_list_array(struct mrsh_array *array,
		const char *prefix) {
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *l = array->data[i];
		bool last = i == array->len - 1;

		char sub_prefix[make_sub_prefix(prefix, last, NULL)];
		make_sub_prefix(prefix, last, sub_prefix);

		print_prefix(prefix, last);
		print_command_list(l, sub_prefix);
	}
}

static void print_brace_group(struct mrsh_brace_group *bg, const char *prefix) {
	printf("brace_group\n");

	print_command_list_array(&bg->body, prefix);
}

static void print_command(struct mrsh_command *cmd, const char *prefix);

static void print_if_clause(struct mrsh_if_clause *ic, const char *prefix) {
	printf("if_clause\n");

	char sub_prefix[make_sub_prefix(prefix, false, NULL)];
	make_sub_prefix(prefix, false, sub_prefix);

	print_prefix(prefix, false);
	printf("condition\n");
	print_command_list_array(&ic->condition, sub_prefix);

	bool last = ic->else_part == NULL;
	make_sub_prefix(prefix, last, sub_prefix);

	print_prefix(prefix, last);
	printf("body\n");
	print_command_list_array(&ic->body, sub_prefix);

	if (ic->else_part != NULL) {
		make_sub_prefix(prefix, true, sub_prefix);

		print_prefix(prefix, true);
		printf("else_part ─ ");
		print_command(ic->else_part, sub_prefix);
	}
}

static void print_command(struct mrsh_command *cmd, const char *prefix) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		assert(sc != NULL);
		print_simple_command(sc, prefix);
		break;
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		assert(bg != NULL);
		print_brace_group(bg, prefix);
		break;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		assert(ic != NULL);
		print_if_clause(ic, prefix);
		break;
	}
}

static void print_pipeline(struct mrsh_pipeline *pl, const char *prefix) {
	printf("pipeline%s\n", pl->bang ? " !" : "");

	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		bool last = i == pl->commands.len - 1;

		char sub_prefix[make_sub_prefix(prefix, last, NULL)];
		make_sub_prefix(prefix, last, sub_prefix);

		print_prefix(prefix, last);
		print_command(cmd, sub_prefix);
	}
}

static const char *binop_type_str(enum mrsh_binop_type type) {
	switch (type) {
	case MRSH_BINOP_AND:
		return "&&";
	case MRSH_BINOP_OR:
		return "||";
	}
	return NULL;
}

static void print_node(struct mrsh_node *node, const char *prefix);

static void print_binop(struct mrsh_binop *binop, const char *prefix) {
	printf("binop %s\n", binop_type_str(binop->type));

	char sub_prefix[make_sub_prefix(prefix, false, NULL)];

	make_sub_prefix(prefix, false, sub_prefix);
	print_prefix(prefix, false);
	print_node(binop->left, sub_prefix);

	make_sub_prefix(prefix, true, sub_prefix);
	print_prefix(prefix, true);
	print_node(binop->right, sub_prefix);
}

static void print_node(struct mrsh_node *node, const char *prefix) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl != NULL);
		print_pipeline(pl, prefix);
		break;
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		assert(binop != NULL);
		print_binop(binop, prefix);
		break;
	}
}

static void print_command_list(struct mrsh_command_list *l,
		const char *prefix) {
	printf("command_list%s ─ ", l->ampersand ? " &" : "");

	print_node(l->node, prefix);
}

void mrsh_program_print(struct mrsh_program *prog) {
	printf("program\n");

	print_command_list_array(&prog->commands, "");
}
