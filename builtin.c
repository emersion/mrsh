#include <assert.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char exit_usage[] = "usage: exit [n]\n";

static int builtin_exit(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, exit_usage);
		return EXIT_FAILURE;
	}

	int status = 0;
	if (argc > 1) {
		char *endptr;
		status = strtol(argv[1], &endptr, 10);
		if (endptr[0] != '\0' || status < 0 || status > 255) {
			fprintf(stderr, exit_usage);
			return EXIT_FAILURE;
		}
	}

	state->exit = status;
	return EXIT_SUCCESS;
}

int mrsh_builtin(struct mrsh_state *state, int argc, char *argv[]) {
	assert(argc > 0);

	const char *cmd = argv[0];
	if (strcmp(cmd, "exit") == 0) {
		return builtin_exit(state, argc, argv);
	}

	return -1;
}
