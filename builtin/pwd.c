#define _POSIX_C_SOURCE 200809L
#include <mrsh/getopt.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"

static const char pwd_usage[] = "usage: pwd [-L|-P]\n";

int builtin_pwd(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":LP")) != -1) {
		switch (opt) {
		case 'L':
		case 'P':
			// TODO implement `-L` and `-P`
			fprintf(stderr, "pwd: `-L` and `-P` not yet implemented\n");
			return 1;
		default:
			fprintf(stderr, "pwd: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, pwd_usage);
			return 1;
		}
	}
	if (mrsh_optind < argc) {
		fprintf(stderr, pwd_usage);
		return 1;
	}

	const char *pwd = mrsh_env_get(state, "PWD", NULL);
	if (pwd == NULL) {
		fprintf(stderr, "pwd: Cannot return current directory as PWD was unset\n");
		return 1;
	}
	puts(pwd);
	return 0;
}
