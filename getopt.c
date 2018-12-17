#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <mrsh/getopt.h>
#include <stdio.h>

char *mrsh_optarg = NULL;
int mrsh_optind = 1;
int mrsh_opterr = 1;
int mrsh_optopt = 0;

int mrsh_getopt(int argc, char *const argv[], const char *optstring) {
	assert(argv[argc] == NULL);
	mrsh_optarg = NULL;

	if (mrsh_optind >= argc) {
		return -1;
	}

	if (argv[mrsh_optind][0] != '-') {
		return -1;
	}

	if (argv[mrsh_optind][1] == '\0') {
		return -1;
	}

	if (argv[mrsh_optind][1] == '-') {
		mrsh_optind++;
		return -1;
	}

	const char *c = optstring;
	if (*c == ':') {
		c++;
	}

	mrsh_optopt = 0;
	int opt = argv[mrsh_optind][1];
	for (; *c != '\0'; c++) {
		if (*c != opt) {
			continue;
		}

		if (c[1] != ':') {
			mrsh_optind++;
			return opt;
		}

		if (argv[mrsh_optind][2] != '\0') {
			mrsh_optarg = &argv[mrsh_optind][2]; 
		} else {
			if (mrsh_optind + 2 > argc) {
				mrsh_optopt = opt;
				if (mrsh_opterr != 0 && optstring[0] != ':') {
					fprintf(stderr, "%s: Option '%c' requires an argument.\n",
						argv[0], mrsh_optopt);
				}

				return optstring[0] == ':' ? ':' : '?';
			}

			mrsh_optarg = argv[++mrsh_optind];
		}

		mrsh_optind++;
		return opt;
	}

	if (mrsh_opterr != 0 && optstring[0] != ':') {
		fprintf(stderr, "%s: Option '%c' not found.\n", argv[0], opt);
	}

	return '?';
}
