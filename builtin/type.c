#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <shell/path.h>
#include <stdlib.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "shell/shell.h"

static const char type_usage[] = "usage: type name...\n";

int builtin_type(struct mrsh_state *state, int argc, char *argv[]) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	_mrsh_optind = 0;
	if (_mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "type: unknown option -- %c\n", _mrsh_optopt);
		fprintf(stderr, type_usage);
		return 1;
	}
	if (_mrsh_optind == argc) {
		fprintf(stderr, type_usage);
		return 1;
	}

	bool error = false;
	for (int i = _mrsh_optind; i < argc; ++i) {
		char *name = argv[i];

		char *alias = mrsh_hashtable_get(&priv->aliases, name);
		if (alias != NULL) {
			fprintf(stdout, "%s is an alias for %s\n", name, alias);
			continue;
		}

		if (mrsh_has_special_builtin(name)) {
			fprintf(stdout, "%s is a special shell builtin\n", name);
			continue;
		}

		if (mrsh_has_builtin(name)) {
			fprintf(stdout, "%s is a shell builtin\n", name);
			continue;
		}

		const char *path = expand_path(state, name, true, false);
		if (path) {
			fprintf(stdout, "%s is %s\n", name, path);
			continue;
		}

		fprintf(stdout, "%s: not found\n", name);
		error = true;
	}

	return error ? 1 : 0;
}
