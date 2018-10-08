#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mrsh/builtin.h>
#include "builtin.h"

static const char unalias_usage[] = "usage: unalias <-a|alias-name...>\n";

static void delete_alias_iterator(const char *key, void *_value, void *user_data) {
	free(mrsh_hashtable_del((struct mrsh_hashtable*)user_data, key));
}

int builtin_unalias(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc == 1) {
		fprintf(stderr, unalias_usage);
		return EXIT_FAILURE;
	}
	
	if (strcmp(argv[1], "-a") == 0) {
		if (argc > 2) {
			fprintf(stderr, unalias_usage);
			return EXIT_FAILURE;
		}
		mrsh_hashtable_for_each(&state->aliases, delete_alias_iterator, &state->aliases);
		return EXIT_SUCCESS;
	}

	for (int i = 1; i < argc; ++i) {
		free(mrsh_hashtable_del(&state->aliases, argv[i]));
	}
	return EXIT_SUCCESS;
}
