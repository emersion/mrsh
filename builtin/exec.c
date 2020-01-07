#include <mrsh/getopt.h>
#include <stdio.h>
#include <unistd.h>
#include "builtin.h"
#include "shell/path.h"

static const char exec_usage[] = "usage: exec [command [argument...]]\n";

int builtin_exec(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	if (mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "exec: unknown option -- %c\n", mrsh_optopt);
		fprintf(stderr, exec_usage);
		return 1;
	}
	if (mrsh_optind == argc) {
		return 0;
	}

	const char *path = expand_path(state, argv[mrsh_optind], false, false);
	if (path == NULL) {
		fprintf(stderr, "exec: %s: command not found\n", argv[mrsh_optind]);
		return 127;
	}
	if (access(path, X_OK) != 0) {
		fprintf(stderr, "exec: %s: not executable\n", path);
		return 126;
	}

	execv(path, &argv[mrsh_optind]);
	perror("exec");
	return 1;
}
