#define _POSIX_C_SOURCE 2
#include <mrsh/ast.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <unistd.h>

const char usage[] = "usage: mrsh [-n]";

int main(int argc, char *argv[]) {
	bool noexec = false;

	int opt;
	while ((opt = getopt(argc, argv, "n")) != -1) {
		switch (opt) {
		case 'n':
			noexec = true;
			break;
		default:
			fprintf(stderr, usage);
			exit(EXIT_FAILURE);
		}
	}

	if (noexec) {
		struct mrsh_program *prog = mrsh_parse(stdin);
		mrsh_program_print(prog);
		return EXIT_SUCCESS;
	}

	struct mrsh_parser *parser = mrsh_parser_create(stdin);
	struct mrsh_state state = {};

	struct mrsh_command_list *list;
	while ((list = mrsh_parse_command_list(parser)) != NULL) {
		mrsh_run_command_list(&state, list);
	}

	mrsh_parser_destroy(parser);
	return EXIT_SUCCESS;
}
