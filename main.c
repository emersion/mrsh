#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/ast.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char usage[] = "usage: mrsh [-n]\n";

int main(int argc, char *argv[]) {
	bool noexec = false;

	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	FILE *input = stdin;

	int opt;
	while ((opt = getopt(argc, argv, "c:ns")) != -1) {
		switch (opt) {
		case 'n':
			noexec = true;
			break;
		case 'c':
			input = fmemopen(optarg, strlen(optarg), "r");
			if (!input) {
				fprintf(stderr, "fmemopen failed: %s", strerror(errno));
				return EXIT_FAILURE;
			}
			break;
		case 's':
			input = stdin;
			break;
		default:
			fprintf(stderr, usage);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		input = fopen(argv[optind], "r");
		if (!input) {
			fprintf(stderr, "could not open %s for reading: %s",
					argv[optind], strerror(errno));
			return EXIT_FAILURE;
		}
		assert(optind + 1 >= argc && "additional args not yet supported");
	}

	struct mrsh_parser *parser = mrsh_parser_create(input);
	while (state.exit == -1) {
		struct mrsh_program *prog = mrsh_parse_line(parser);
		if (prog == NULL) {
			state.exit = EXIT_SUCCESS;
			break;
		}
		mrsh_program_print(prog);
		if (!noexec) {
			mrsh_run_program(&state, prog);
		}
		mrsh_program_destroy(prog);
	}

	mrsh_parser_destroy(parser);
	fclose(input);
	return state.exit;
}
