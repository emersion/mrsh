#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"

#define L_LINE "│ "
#define L_VAL  "├─"
#define L_LAST "└─"
#define L_GAP  "  "

size_t make_sub_prefix(const char *prefix, bool last, char *buf) {
	if (buf != NULL) {
		memcpy(buf, prefix, strlen(prefix) + 1);
		strcat(buf, last ? L_GAP : L_LINE);
	}
	return strlen(prefix) + strlen(L_LINE) + 1;
}

static void print_io_redirect(struct mrsh_io_redirect *redir) {
	printf("io_redirect %d %s %s\n", redir->io_number, redir->op, redir->filename);
}

static void print_command(struct mrsh_command *cmd, const char *prefix) {
	printf("command %s", cmd->name);
	for (size_t i = 0; i < cmd->arguments.len; ++i) {
		char *arg = cmd->arguments.data[i];
		printf(" %s", arg);
	}
	printf("\n");

	for (size_t i = 0; i < cmd->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = cmd->io_redirects.data[i];
		bool last = i == cmd->io_redirects.len - 1 && cmd->assignments.len == 0;
		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		print_io_redirect(redir);
	}

	for (size_t i = 0; i < cmd->assignments.len; ++i) {
		char *assign = cmd->assignments.data[i];
		bool last = i == cmd->assignments.len - 1;
		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		printf("assignment %s\n", assign);
	}
}

static void print_pipeline(struct mrsh_pipeline *pl, const char *prefix) {
	printf("pipeline\n");

	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		bool last = i == pl->commands.len - 1;

		char sub_prefix[make_sub_prefix(prefix, last, NULL)];
		make_sub_prefix(prefix, last, sub_prefix);

		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		print_command(cmd, sub_prefix);
	}
}

static void print_node(struct mrsh_node *node, const char *prefix);

static void print_binop(struct mrsh_binop *binop, const char *prefix) {
	printf("binop\n");

	char sub_prefix[make_sub_prefix(prefix, false, NULL)];

	make_sub_prefix(prefix, false, sub_prefix);
	printf("%s%s", prefix, L_VAL);
	print_node(binop->left, sub_prefix);

	make_sub_prefix(prefix, true, sub_prefix);
	printf("%s%s", prefix, L_LAST);
	print_node(binop->right, sub_prefix);
}

static void print_node(struct mrsh_node *node, const char *prefix) {
	struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
	if (pl != NULL) {
		print_pipeline(pl, prefix);
		return;
	}

	struct mrsh_binop *binop = mrsh_node_get_binop(node);
	if (binop != NULL) {
		print_binop(binop, prefix);
		return;
	}

	printf("unknown node\n");
}

static void print_command_list(struct mrsh_command_list *l,
		const char *prefix) {
	printf("command_list%s: ", l->ampersand ? " &" : "");

	print_node(l->node, prefix);
}

static void print_program(struct mrsh_program *prog) {
	printf("program\n");

	for (size_t i = 0; i < prog->commands.len; ++i) {
		struct mrsh_command_list *l = prog->commands.data[i];
		bool last = i == prog->commands.len - 1;
		printf(last ? L_LAST : L_VAL);
		print_command_list(l, last ? L_GAP : L_LINE);
	}
}

int main(int argc, char *argv[]) {
	struct mrsh_program *prog = mrsh_parse(stdin);
	print_program(prog);
	return EXIT_SUCCESS;
}
