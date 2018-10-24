#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <mrsh/buffer.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"

static const char getopts_usage[] = "usage: getopts optstring name [arg...]\n";

int builtin_getopts(struct mrsh_state *state, int argc, char *argv[]) {
	optind = 1;
	if (getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "getopts: unknown option -- %c\n", optopt);
		fprintf(stderr, getopts_usage);
		return EXIT_FAILURE;
	}
	if (optind + 2 < argc) {
		fprintf(stderr, getopts_usage);
		return EXIT_FAILURE;
	}

	int optc;
	char **optv;
	if (optind + 2 > argc) {
		optc = argc - optind - 2;
		optv = &argv[optind + 2];
	} else {
		optc = state->argc;
		optv = state->argv;
	}
	char *optstring = argv[optind];
	char *name = argv[optind + 1];

	const char *optind_str = mrsh_env_get(state, "OPTIND", NULL);
	if (optind_str == NULL) {
		fprintf(stderr, "getopts: OPTIND is not defined\n");
		return EXIT_FAILURE;
	}
	char *endptr;
	long optind_long = strtol(optind_str, &endptr, 10);
	if (endptr[0] != '\0' || optind_long <= 0 || optind_long > INT_MAX) {
		fprintf(stderr, "getopts: OPTIND is not a positive integer\n");
		return EXIT_FAILURE;
	}
	optind = (int)optind_long;

	optopt = 0;
	int opt = getopt(optc, optv, optstring);

	char optind_fmt[16];
	snprintf(optind_fmt, sizeof(optind_fmt), "%d", optind);
	mrsh_env_set(state, "OPTIND", optind_fmt, MRSH_VAR_ATTRIB_NONE);

	if (optopt != 0) {
		if (opt == ':') {
			char value[] = {(char)optopt, '\0'};
			mrsh_env_set(state, "OPTARG", value, MRSH_VAR_ATTRIB_NONE);
		} else if (optstring[0] != ':') {
			mrsh_env_unset(state, "OPTARG");
		} else {
			// either missing option-argument or unknown option character
			// in the former case, unset OPTARG
			// in the latter case, set OPTARG to optopt
			bool opt_exists = false;
			size_t len = strlen(optstring);
			for (size_t i = 0; i < len; ++i) {
				if (optstring[i] == optopt) {
					opt_exists = true;
					break;
				}
			}
			if (opt_exists) {
				mrsh_env_unset(state, "OPTARG");
			} else {
				char value[] = {(char)optopt, '\0'};
				mrsh_env_set(state, "OPTARG", value, MRSH_VAR_ATTRIB_NONE);
			}
		}
	} else if (optarg != NULL) {
		mrsh_env_set(state, "OPTARG", optarg, MRSH_VAR_ATTRIB_NONE);
	} else {
		mrsh_env_unset(state, "OPTARG");
	}

	char value[] = {opt == -1 ? (char)'?' : (char)opt, '\0'};
	mrsh_env_set(state, name, value, MRSH_VAR_ATTRIB_NONE);

	if (opt == -1) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
