#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <mrsh/getopt.h>
#include <mrsh/shell.h>
#include <shell/path.h>
#include <stdlib.h>
#include "builtin.h"

static const char type_usage[] = "usage: type name...\n";

int builtin_type(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	if (mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "type: unknown option -- %c\n", mrsh_optopt);
		fprintf(stderr, type_usage);
		return 1;
	}
	if (mrsh_optind == argc) {
		fprintf(stderr, type_usage);
		return 1;
	}

	bool error = false;
	for (int i = mrsh_optind; i < argc; ++i) {
		char *name = argv[i];

		char *alias = mrsh_hashtable_get(&state->aliases, name);
		if (alias != NULL) {
			fprintf(stdout, "%s is an alias for %s\n", name, alias);
			continue;
		}

		if (mrsh_has_builtin(name)) {
			fprintf(stdout, "%s is a shell builtin\n", name);
			continue;
		}

		const char *path = expand_path(state, name, true);
		if (path) {
			fprintf(stdout, "%s is %s\n", name, path);
			continue;
		}

		fprintf(stdout, "%s: not found\n", name);
		error = true;
	}

	return error ? 1 : 0;
}
