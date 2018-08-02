#include <assert.h>
#include <mrsh/ast.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void print_token(struct mrsh_token *token, const char *prefix) {
	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);
		printf("token_string%s %s\n",
			ts->single_quoted ? " (quoted)" : "", ts->str);
		break;
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		printf("token_list%s\n", tl->double_quoted ? " (quoted)" : "");

		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			bool last = i == tl->children.len - 1;

			char sub_prefix[make_sub_prefix(prefix, last, NULL)];
			make_sub_prefix(prefix, last, sub_prefix);

			print_prefix(prefix, last);
			print_token(child, sub_prefix);
		}
		break;
	}
}

static void print_io_redirect(struct mrsh_io_redirect *redir,
		const char *prefix) {
	printf("io_redirect\n");

	print_prefix(prefix, false);
	printf("io_number %d\n", redir->io_number);

	print_prefix(prefix, false);
	printf("op %s\n", redir->op);

	char sub_prefix[make_sub_prefix(prefix, true, NULL)];
	make_sub_prefix(prefix, true, sub_prefix);

	print_prefix(prefix, true);
	printf("filename ─ ");
	print_token(redir->filename, sub_prefix);
}

static void print_assignment(struct mrsh_assignment *assign,
		const char *prefix) {
	printf("assignment\n");

	print_prefix(prefix, false);
	printf("name %s\n", assign->name);

	char sub_prefix[make_sub_prefix(prefix, true, NULL)];
	make_sub_prefix(prefix, true, sub_prefix);

	print_prefix(prefix, true);
	printf("value ─ ");
	print_token(assign->value, sub_prefix);
}

static void print_simple_command(struct mrsh_simple_command *cmd,
		const char *prefix) {
	printf("simple_command\n");

	bool last = cmd->arguments.len == 0 && cmd->io_redirects.len == 0
		&& cmd->assignments.len == 0;
	char sub_prefix[make_sub_prefix(prefix, last, NULL)];
	make_sub_prefix(prefix, last, sub_prefix);

	print_prefix(prefix, last);
	printf("name ─ ");
	print_token(cmd->name, sub_prefix);

	for (size_t i = 0; i < cmd->arguments.len; ++i) {
		struct mrsh_token *arg = cmd->arguments.data[i];
		bool last = i == cmd->arguments.len - 1 && cmd->io_redirects.len == 0
			&& cmd->assignments.len == 0;

		make_sub_prefix(prefix, last, sub_prefix);

		print_prefix(prefix, last);
		printf("argument %zu ─ ", i + 1);
		print_token(arg, sub_prefix);
	}

	for (size_t i = 0; i < cmd->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = cmd->io_redirects.data[i];
		bool last = i == cmd->io_redirects.len - 1 && cmd->assignments.len == 0;

		make_sub_prefix(prefix, last, sub_prefix);

		print_prefix(prefix, last);
		print_io_redirect(redir, sub_prefix);
	}

	for (size_t i = 0; i < cmd->assignments.len; ++i) {
		struct mrsh_assignment *assign = cmd->assignments.data[i];
		bool last = i == cmd->assignments.len - 1;

		make_sub_prefix(prefix, last, sub_prefix);

		print_prefix(prefix, last);
		print_assignment(assign, sub_prefix);
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

static void print_command_list(struct mrsh_command_list *list,
		const char *prefix) {
	printf("command_list%s ─ ", list->ampersand ? " &" : "");

	print_node(list->node, prefix);
}

void mrsh_program_print(struct mrsh_program *prog) {
	printf("program\n");

	print_command_list_array(&prog->body, "");
}
