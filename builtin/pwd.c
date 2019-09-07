#define _POSIX_C_SOURCE 200809L
#include <assert.h>
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
			/* This space deliberately left blank */
			break;
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
	assert(pwd != NULL);
	puts(pwd);

	return 0;
}
