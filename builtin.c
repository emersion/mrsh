#include <assert.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*mrsh_builtin_func_t)(struct mrsh_state *state,
	int argc, char *argv[]);

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

mrsh_builtin_func_t get_builtin(const char *name) {
	if (strcmp(name, "exit") == 0) {
		return builtin_exit;
	} else {
		return NULL;
	}
}

int mrsh_has_builtin(const char *name) {
	return get_builtin(name) != NULL;
}

int mrsh_run_builtin(struct mrsh_state *state, int argc, char *argv[]) {
	assert(argc > 0);

	const char *name = argv[0];
	mrsh_builtin_func_t builtin = get_builtin(name);
	if (builtin == NULL) {
		return -1;
	}

	return builtin(state, argc, argv);
}
