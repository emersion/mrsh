#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <mrsh/getopt.h>
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

	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":a")) != -1) {
		switch (opt) {
		case 'a':
			all = true;
			break;
		default:
			fprintf(stderr, "unalias: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, unalias_usage);
			return 1;
		}
	}

	if (all) {
		if (mrsh_optind < argc) {
			fprintf(stderr, unalias_usage);
			return 1;
		}
		mrsh_hashtable_for_each(&state->aliases, delete_alias_iterator, &state->aliases);
		return 0;
	}

	if (mrsh_optind == argc) {
		fprintf(stderr, unalias_usage);
		return 1;
	}

	for (int i = mrsh_optind; i < argc; ++i) {
		free(mrsh_hashtable_del(&state->aliases, argv[i]));
	}
	return 0;
}
