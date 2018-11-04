#include <errno.h>
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
		errno = 0;
		long n_long = strtol(argv[1], &endptr, 10);
		if (*endptr != '\0' || errno != 0) {
			fprintf(stderr, shift_usage);
			if (!state->interactive) {
				state->exit = EXIT_FAILURE;
			}
			return EXIT_FAILURE;
		}
		n = (int)n_long;
	}
	if (n == 0) {
		return EXIT_SUCCESS;
	} else if (n < 1) {
		fprintf(stderr, "shift: [n] must be positive\n");
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	} else if (n > state->args->argc - 1) {
		fprintf(stderr, "shift: [n] must be less than $#\n");
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	}
	for (int i = 1, j = n + 1; j < state->args->argc; ++i, ++j) {
		if (j <= state->args->argc - n) {
			state->args->argv[i] = state->args->argv[j];
		} else {
			free(state->args->argv[i]);
		}
	}
	state->args->argc -= n;
	return EXIT_SUCCESS;
}
