#include <mrsh/builtin.h>
#include <mrsh/shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char set_usage[] =
	"usage: set [(-|+)abCefhmnuvx] [-o option] [args...]\n";

struct option_map {
	const char *name;
	char short_name;
	enum mrsh_option value;
};

static const struct option_map options[] = {
	{ "allexport", 'a', MRSH_OPT_ALLEXPORT },
	{ "notify", 'b', MRSH_OPT_NOTIFY },
	{ "noclobber", 'C', MRSH_OPT_NOCLOBBER },
	{ "errexit", 'e', MRSH_OPT_ERREXIT },
	{ "noglob", 'f', MRSH_OPT_NOGLOB },
	{ NULL, 'h', MRSH_OPT_PRELOOKUP },
	{ "monitor", 'm', MRSH_OPT_MONITOR },
	{ "noexec", 'n', MRSH_OPT_NOEXEC },
	{ "ignoreeof", 0, MRSH_OPT_IGNOREEOF },
	{ "nolog", 0, MRSH_OPT_NOLOG },
	{ "vi", 0, MRSH_OPT_VI },
	{ "nounset", 'u', MRSH_OPT_NOUNSET },
	{ "verbose", 'v', MRSH_OPT_VERBOSE },
	{ "xtrace", 'x', MRSH_OPT_XTRACE },
};

static const struct option_map *find_option(char opt) {
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		if (options[i].short_name && options[i].short_name == opt) {
			return &options[i];
		}
	}
	return NULL;
}

static const struct option_map *find_long_option(const char *opt) {
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		if (options[i].name && strcmp(options[i].name, opt) == 0) {
			return &options[i];
		}
	}
	return NULL;
}

int builtin_set(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc == 1) {
		// TODO: Print all shell variables
		return EXIT_FAILURE;
	}

	bool force_positional = false;
	int i;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--") == 0) {
			force_positional = true;
			break;
		}
		if (argv[i][0] != '-' && argv[i][0] != '+') {
			break;
		}
		if (argv[i][1] == '\0') {
			fprintf(stderr, set_usage);
			return EXIT_FAILURE;
		}
		const struct option_map *option;
		if (argv[i][1] == 'o') {
			if (i + 1 == argc) {
				fprintf(stderr, set_usage);
				return EXIT_FAILURE;
			}
			option = find_long_option(argv[i + 1]);
			if (!option) {
				fprintf(stderr, set_usage);
				return EXIT_FAILURE;
			}
			if (argv[i][0] == '-') {
				state->options |= option->value;
			} else {
				state->options &= ~option->value;
			}
			++i;
			continue;
		}
		for (int j = 1; argv[i][j]; ++j) {
			option = find_option(argv[i][j]);
			if (!option) {
				fprintf(stderr, set_usage);
				return EXIT_FAILURE;
			}
			if (argv[i][0] == '-') {
				state->options |= option->value;
			} else {
				state->options &= ~option->value;
			}
		}
	}

	if (i != argc || force_positional) {
		// TODO: Assign remaining arguments to positional parameters, and set $#
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
