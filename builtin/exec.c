#include <stdio.h>
#include <unistd.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "shell/path.h"

static const char exec_usage[] = "usage: exec [command [argument...]]\n";

int builtin_exec(struct mrsh_state *state, int argc, char *argv[]) {
	_mrsh_optind = 0;
	if (_mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "exec: unknown option -- %c\n", _mrsh_optopt);
		fprintf(stderr, exec_usage);
		return 1;
	}
	if (_mrsh_optind == argc) {
		return 0;
	}

	const char *path = expand_path(state, argv[_mrsh_optind], false, false);
	if (path == NULL) {
		fprintf(stderr, "exec: %s: command not found\n", argv[_mrsh_optind]);
		return 127;
	}
	if (access(path, X_OK) != 0) {
		fprintf(stderr, "exec: %s: not executable\n", path);
		return 126;
	}

	execv(path, &argv[_mrsh_optind]);
	perror("exec");
	return 1;
}
