#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include "mrsh_getopt.h"

char *_mrsh_optarg = NULL;
int _mrsh_optind = 1;
int _mrsh_opterr = 1;
int _mrsh_optopt = 0;
int _mrsh_optpos = 1;

int _mrsh_getopt(int argc, char *const argv[], const char *optstring) {
	assert(argv[argc] == NULL);
	_mrsh_optarg = NULL;

	if (_mrsh_optind == 0) {
		_mrsh_optind = 1;
		_mrsh_optpos = 1;
	}

	if (_mrsh_optind >= argc) {
		return -1;
	}

	if (argv[_mrsh_optind][0] != '-') {
		return -1;
	}

	if (argv[_mrsh_optind][1] == '\0') {
		return -1;
	}

	if (argv[_mrsh_optind][1] == '-') {
		_mrsh_optind++;
		return -1;
	}

	const char *c = optstring;
	if (*c == ':') {
		c++;
	}

	_mrsh_optopt = 0;
	int opt = argv[_mrsh_optind][_mrsh_optpos];
	for (; *c != '\0'; c++) {
		if (*c != opt) {
			continue;
		}

		if (c[1] != ':') {
			if (argv[_mrsh_optind][_mrsh_optpos + 1] == '\0') {
				_mrsh_optind++;
				_mrsh_optpos = 1;
			} else {
				_mrsh_optpos++;
			}
			return opt;
		}

		if (argv[_mrsh_optind][_mrsh_optpos + 1] != '\0') {
			_mrsh_optarg = &argv[_mrsh_optind][_mrsh_optpos + 1];
		} else {
			if (_mrsh_optind + 2 > argc) {
				_mrsh_optopt = opt;
				if (_mrsh_opterr != 0 && optstring[0] != ':') {
					fprintf(stderr, "%s: Option '%c' requires an argument.\n",
						argv[0], _mrsh_optopt);
				}

				return optstring[0] == ':' ? ':' : '?';
			}

			_mrsh_optarg = argv[++_mrsh_optind];
		}

		_mrsh_optind++;
		return opt;
	}

	if (_mrsh_opterr != 0 && optstring[0] != ':') {
		fprintf(stderr, "%s: Option '%c' not found.\n", argv[0], opt);
	}

	return '?';
}
