#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mrsh.h"
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
	printf("command %s\n", cmd->name);

	for (size_t i = 0; i < cmd->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = cmd->io_redirects.data[i];
		bool last = i == cmd->io_redirects.len - 1;
		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		print_io_redirect(redir);
	}
}

static void print_pipeline(struct mrsh_pipeline *pipeline,
		const char *prefix) {
	printf("pipeline\n");

	for (size_t i = 0; i < pipeline->commands.len; ++i) {
		struct mrsh_command *cmd = pipeline->commands.data[i];
		bool last = i == pipeline->commands.len - 1;

		char sub_prefix[make_sub_prefix(prefix, last, NULL)];
		make_sub_prefix(prefix, last, sub_prefix);

		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		print_command(cmd, sub_prefix);
	}
}

static void print_complete_command(struct mrsh_complete_command *cmd,
		const char *prefix) {
	printf("complete_command\n");

	for (size_t i = 0; i < cmd->pipelines.len; ++i) {
		struct mrsh_pipeline *pipeline = cmd->pipelines.data[i];
		bool last = i == cmd->pipelines.len - 1;

		char sub_prefix[make_sub_prefix(prefix, last, NULL)];
		make_sub_prefix(prefix, last, sub_prefix);

		printf("%s%s", prefix, last ? L_LAST : L_VAL);
		print_pipeline(pipeline, sub_prefix);
	}
}

static void print_program(struct mrsh_program *prog) {
	printf("program\n");

	for (size_t i = 0; i < prog->commands.len; ++i) {
		struct mrsh_complete_command *cmd = prog->commands.data[i];
		bool last = i == prog->commands.len - 1;
		printf(last ? L_LAST : L_VAL);
		print_complete_command(cmd, last ? L_GAP : L_LINE);
	}
}

int main(int argc, char *argv[]) {
	struct mrsh_program *prog = mrsh_parse(stdin);
	print_program(prog);
	return EXIT_SUCCESS;
}
