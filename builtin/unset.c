#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char unset_usage[] = "usage: unset [-fv] name...\n";

int builtin_unset(struct mrsh_state *state, int argc, char *argv[]) {
	bool funcs = false;

	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":fv")) != -1) {
		switch (opt) {
		case 'f':
			funcs = true;
			break;
		case 'v':
			funcs = false;
			break;
		default:
			fprintf(stderr, "unset: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, unset_usage);
			return 1;
		}
	}
	if (mrsh_optind >= argc) {
		fprintf(stderr, unset_usage);
		return 1;
	}
	for (int i = mrsh_optind; i < argc; ++i) {
		if (!funcs) {
			uint32_t prev_attribs = 0;
			if (mrsh_env_get(state, argv[i], &prev_attribs)) {
				if ((prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
					fprintf(stderr,
						"unset: cannot modify readonly variable %s\n", argv[i]);
					return 1;
				}
				mrsh_env_unset(state, argv[i]);
			}
		} else {
			struct mrsh_function *oldfn =
				mrsh_hashtable_del(&state->functions, argv[i]);
			mrsh_function_destroy(oldfn);
		}
	}
	return 0;
}
