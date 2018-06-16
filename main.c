#define _POSIX_C_SOURCE 2
#include <stdlib.h>
#include <unistd.h>

#include "ast.h"
#include "parser.h"
#include "shell.h"

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

	struct mrsh_program *prog = mrsh_parse(stdin);

	if (noexec) {
		mrsh_program_print(prog);
		return EXIT_SUCCESS;
	}

	struct mrsh_state state = {};
	mrsh_run_program(&state, prog);
	return EXIT_SUCCESS;
}
