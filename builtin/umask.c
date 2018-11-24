#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "builtin.h"

/*
 * TODO: Implement -S flag and symbolic mode
 */

static const char umask_usage[] = "usage: umask [mode]\n";

int builtin_umask(struct mrsh_state *state, int argc, char *argv[]) {
	mode_t mode;
	mode_t default_mode = 0022;

	if (argc < 2) {
		mode = umask(default_mode);
		printf("%04o\n", mode);
		umask(mode);
		return EXIT_SUCCESS;
	}

	char *endptr;
	mode = strtol(argv[1], &endptr, 8);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid mode %s\n", argv[1]);
		fprintf(stderr, umask_usage);
		return EXIT_FAILURE;
	}

	umask(mode);
	return EXIT_SUCCESS;
}
