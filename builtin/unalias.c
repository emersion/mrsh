#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char unalias_usage[] = "usage: unalias -a|alias-name...\n";

static void delete_alias_iterator(const char *key, void *_value, void *user_data) {
	free(mrsh_hashtable_del((struct mrsh_hashtable*)user_data, key));
}

int builtin_unalias(struct mrsh_state *state, int argc, char *argv[]) {
	bool all = false;

	optind = 1;
	int opt;
	while ((opt = getopt(argc, argv, ":a")) != -1) {
		switch (opt) {
		case 'a':
			all = true;
			break;
		default:
			fprintf(stderr, "unalias: unknown option -- %c\n", optopt);
			fprintf(stderr, unalias_usage);
			return EXIT_FAILURE;
		}
	}

	if (all) {
		if (optind < argc) {
			fprintf(stderr, unalias_usage);
			return EXIT_FAILURE;
		}
		mrsh_hashtable_for_each(&state->aliases, delete_alias_iterator, &state->aliases);
		return EXIT_SUCCESS;
	}

	if (optind == argc) {
		fprintf(stderr, unalias_usage);
		return EXIT_FAILURE;
	}

	for (int i = optind; i < argc; ++i) {
		free(mrsh_hashtable_del(&state->aliases, argv[i]));
	}
	return EXIT_SUCCESS;
}
