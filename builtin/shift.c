#include <mrsh/builtin.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include "builtin.h"

static const char shift_usage[] = "usage: shift [n]\n";

int builtin_shift(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, shift_usage);
		return EXIT_FAILURE;
	}
	int n = 1;
	if (argc == 2) {
		char *endptr;
		n = strtol(argv[1], &endptr, 10);
		if (*endptr != '\0') {
			fprintf(stderr, shift_usage);
			if (!state->interactive) {
				state->exit = EXIT_FAILURE;
			}
			return EXIT_FAILURE;
		}
	}
	if (n == 0) {
		return EXIT_SUCCESS;
	} else if (n < 1) {
		fprintf(stderr, "shift: [n] must be positive\n");
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	} else if (n > state->argc - 1) {
		fprintf(stderr, "shift: [n] must be less than $#\n");
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	}
	for (int i = 1, j = n + 1; j < state->argc; ++i, ++j) {
		if (j <= state->argc - n) {
			state->argv[i] = state->argv[j];
		} else {
			free(state->argv[i]);
		}
	}
	state->argc -= n;
	return EXIT_SUCCESS;
}
