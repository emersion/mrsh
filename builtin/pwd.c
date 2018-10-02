#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <mrsh/shell.h>
#include "builtin.h"

// usage: pwd [-L|-P]

int builtin_pwd(struct mrsh_state *state, int argc, char *argv[]) {
	// TODO `-P` and `-L`
	const char *pwd = mrsh_env_get(state, "PWD", NULL);
	if (pwd == NULL) {
		fprintf(stderr, "pwd: Cannot return current directory as PWD was unset\n");
		return EXIT_FAILURE;
	}
	puts(pwd);
	return EXIT_SUCCESS;
}
