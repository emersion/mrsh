#define _POSIX_C_SOURCE 200809L
#include <errno.h>
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

const char *print_options(struct mrsh_state *state) {
	static char opts[sizeof(options) / sizeof(options[0]) + 1];
	int i = 0;
	for (size_t j = 0; j < sizeof(options) / sizeof(options[0]); ++j) {
		if (options[j].short_name && (state->options & options[j].value)) {
			opts[i++] = options[j].short_name;
		}
	}
	opts[i] = 0;
	return opts;
}

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

static char **argv_dup(char *argv_0, int argc, char *argv[]) {
	char **_argv = malloc((argc + 1) * sizeof(char *));
	_argv[0] = argv_0;
	for (int i = 1; i < argc; ++i) {
		_argv[i] = strdup(argv[i - 1]);
	}
	return _argv;
}

static void argv_free(int argc, char **argv) {
	if (!argv) {
		return;
	}
	for (int i = 0; i < argc; ++i) {
		free(argv[i]);
	}
	free(argv);
}

static int set(struct mrsh_state *state, int argc, char *argv[], bool cmdline) {
	if (argc == 1 && !cmdline) {
		size_t count;
		struct mrsh_collect_var *vars = mrsh_collect_vars(
				state, MRSH_VAR_ATTRIB_NONE, &count);
		for (size_t i = 0; i < count; ++i) {
			printf("%s=", vars[i].key);
			print_escaped(vars[i].value);
			printf("\n");
		}
		free(vars);
		return EXIT_SUCCESS;
	}

	bool force_positional = false;
	int i;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--") == 0) {
			force_positional = true;
			++i;
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
		switch (argv[i][1]) {
		case 'o':
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
		case 'c':
			if (!cmdline) {
				fprintf(stderr, set_usage);
				return EXIT_FAILURE;
			}
			state->input = fmemopen(argv[i + 1], strlen(argv[i + 1]), "r");
			++i;
			if (!state->input) {
				fprintf(stderr, "fmemopen failed: %s", strerror(errno));
				return EXIT_FAILURE;
			}
			break;
		case 's':
			state->input = stdin;
			break;
		default:
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
			break;
		}
	}

	if (i != argc || force_positional) {
		char *argv_0;
		if (cmdline) {
			argv_0 = strdup(argv[i++]);
			state->input = fopen(argv_0, "r");
			// TODO: Turn off -m if the user didn't explicitly set it
			state->interactive = false;
			if (!state->input) {
				fprintf(stderr, "could not open %s for reading: %s\n",
					argv_0, strerror(errno));
				return EXIT_FAILURE;
			}
		} else {
			argv_0 = strdup(state->argv[0]);
		}
		argv_free(state->argc, state->argv);
		state->argc = argc - i + 1;
		state->argv = argv_dup(argv_0, state->argc, &argv[i]);
	} else if (cmdline) {
		// No args given, but we need to initialize state->argv
		state->argc = 1;
		state->argv = argv_dup(strdup(argv[0]), 1, argv);
	}

	return EXIT_SUCCESS;
}

int builtin_set(struct mrsh_state *state, int argc, char *argv[]) {
	return set(state, argc, argv, false);
}

int mrsh_process_args(struct mrsh_state *state, int argc, char *argv[]) {
	return set(state, argc, argv, true);
}
