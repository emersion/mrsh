#define _POSIX_C_SOURCE 200809L
#include <mrsh/getopt.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "builtin.h"

static const char ulimit_usage[] = "usage: ulimit [-f] [blocks]\n";

int builtin_ulimit(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 1;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":f")) != -1) {
		if (opt == 'f') {
			// Nothing here
		} else {
			fprintf(stderr, "%s", ulimit_usage);
			return EXIT_FAILURE;
		}
	}

	if (mrsh_optind == argc - 1) {
		errno = 0;
		char *arg = argv[mrsh_optind];
		char *endptr;
		long int new_limit = strtol(arg, &endptr, 10);
		if (errno != 0) {
			fprintf(stderr, "strtol error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		if ((endptr == arg) || (endptr[0] != '\0')) {
			fprintf(stderr, "ulimit error: Invalid argument: %s\n",
				arg);
			return EXIT_FAILURE;
		}
		struct rlimit new = {
			.rlim_cur = new_limit * 512,
			.rlim_max = new_limit * 512
		};
		if (setrlimit(RLIMIT_FSIZE, &new) != 0) {
			fprintf(stderr, "setrlimit error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
	} else if (mrsh_optind == argc) {
		struct rlimit old = { 0 };
		if (getrlimit(RLIMIT_FSIZE, &old) != 0) {
			fprintf(stderr, "getrlimit error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		if (old.rlim_max == RLIM_INFINITY) {
			printf("unlimited\n");
		} else {
			printf("%lu\n", old.rlim_max / 512);
		}
	} else {
		fprintf(stderr, "%s", ulimit_usage);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
