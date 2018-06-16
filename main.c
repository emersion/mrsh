#define _POSIX_C_SOURCE 2
#include <mrsh/ast.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <unistd.h>

static const char usage[] = "usage: mrsh [-n]\n";

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
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	while (state.exit == -1) {
		struct mrsh_command_list *list = mrsh_parse_command_list(parser);
		if (list == NULL) {
			state.exit = EXIT_SUCCESS;
			break;
		}
		mrsh_run_command_list(&state, list);
	}

	mrsh_parser_destroy(parser);
	return state.exit;
}
