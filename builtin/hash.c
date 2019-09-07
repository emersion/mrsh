#include <mrsh/getopt.h>
#include <mrsh/builtin.h>
#include <shell/path.h>
#include <stdio.h>
#include <string.h>
#include "builtin.h"

static const char hash_usage[] = "usage: hash -r|utility...\n";

int builtin_hash(struct mrsh_state *state, int argc, char *argv[]) {
	/* Hashing and remembering executable location isn't implemented. Thus most
	 * of this builtin just does nothing. */
	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":r")) != -1) {
		switch (opt) {
		case 'r':
			/* no-op: reset list of cached utilities */
			return 0;
		default:
			fprintf(stderr, "hash: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, hash_usage);
			return 1;
		}
	}

	if (argc == 1) {
		/* no-op: print list of cached utilities */
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		const char *utility = argv[i];
		if (strchr(utility, '/') != NULL) {
			fprintf(stderr,
				"hash: undefined behaviour: utility contains a slash\n");
			return 1;
		}

		if (mrsh_has_builtin(utility)) {
			continue;
		}

		const char *path = expand_path(state, utility, true);
		if (path == NULL) {
			fprintf(stderr, "hash: command not found: %s\n", utility);
			return 1;
		}
	}

	return 0;
}
