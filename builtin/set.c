#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <mrsh/builtin.h>
#include <mrsh/shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "shell/shell.h"

static const char set_usage[] =
	"usage: set [(-|+)abCefhmnuvx] [-o option] [args...]\n"
	"       set [(-|+)abCefhmnuvx] [+o option] [args...]\n"
	"       set -- [args...]\n"
	"       set -o\n"
	"       set +o\n";

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

const char *state_get_options(struct mrsh_state *state) {
	static char opts[sizeof(options) / sizeof(options[0]) + 1];
	int i = 0;
	for (size_t j = 0; j < sizeof(options) / sizeof(options[0]); ++j) {
		if (options[j].short_name != '\0' &&
				(state->options & options[j].value)) {
			opts[i++] = options[j].short_name;
		}
	}
	opts[i] = '\0';
	return opts;
}

static void print_options(struct mrsh_state *state) {
	for (size_t j = 0; j < sizeof(options) / sizeof(options[0]); ++j) {
		if (options[j].name != NULL) {
			printf("set %co %s\n",
				(state->options & options[j].value) ? '-' : '+',
				options[j].name);
		}
	}
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

static int set(struct mrsh_state *state, int argc, char *argv[],
		struct mrsh_init_args *init_args, uint32_t *populated_opts) {
	if (argc == 1 && init_args == NULL) {
		size_t count;
		struct mrsh_collect_var *vars = collect_vars(
			state, MRSH_VAR_ATTRIB_NONE, &count);
		for (size_t i = 0; i < count; ++i) {
			printf("%s=", vars[i].key);
			print_escaped(vars[i].value);
			printf("\n");
		}
		free(vars);
		return 0;
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
			return 1;
		}
		const struct option_map *option;
		switch (argv[i][1]) {
		case 'o':
			if (i + 1 == argc) {
				print_options(state);
				return 0;
			}
			option = find_long_option(argv[i + 1]);
			if (!option) {
				fprintf(stderr, set_usage);
				return 1;
			}
			if (argv[i][0] == '-') {
				state->options |= option->value;
			} else {
				state->options &= ~option->value;
			}
			if (populated_opts != NULL) {
				*populated_opts |= option->value;
			}
			++i;
			continue;
		case 'c':
			if (init_args == NULL) {
				fprintf(stderr, set_usage);
				return 1;
			}
			init_args->command_str = argv[i + 1];
			++i;
			break;
		case 's':
			if (init_args == NULL) {
				fprintf(stderr, set_usage);
				return 1;
			}
			init_args->command_str = NULL;
			init_args->command_file = NULL;
			break;
		default:
			for (int j = 1; argv[i][j]; ++j) {
				option = find_option(argv[i][j]);
				if (!option) {
					fprintf(stderr, set_usage);
					return 1;
				}
				if (argv[i][0] == '-') {
					state->options |= option->value;
				} else {
					state->options &= ~option->value;
				}
				if (populated_opts != NULL) {
					*populated_opts |= option->value;
				}
			}
			break;
		}
	}

	if (i != argc || force_positional) {
		char *argv_0;
		if (init_args != NULL) {
			argv_0 = strdup(argv[i++]);
			init_args->command_file = argv_0;
		} else {
			argv_0 = strdup(state->frame->argv[0]);
		}
		argv_free(state->frame->argc, state->frame->argv);
		state->frame->argc = argc - i + 1;
		state->frame->argv = argv_dup(argv_0, state->frame->argc, &argv[i]);
	} else if (init_args != NULL) {
		// No args given, but we need to initialize state->argv
		state->frame->argc = 1;
		state->frame->argv = argv_dup(strdup(argv[0]), 1, argv);
	}

	return 0;
}

int builtin_set(struct mrsh_state *state, int argc, char *argv[]) {
	uint32_t populated_opts = 0;
	int ret = set(state, argc, argv, NULL, &populated_opts);
	if (ret != 0) {
		return ret;
	}

	if (populated_opts & MRSH_OPT_MONITOR) {
		if (!mrsh_set_job_control(state, state->options & MRSH_OPT_MONITOR)) {
			return 1;
		}
	}

	return 0;
}

int mrsh_process_args(struct mrsh_state *state, struct mrsh_init_args *init_args,
		int argc, char *argv[]) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	uint32_t populated_opts = 0;
	int ret = set(state, argc, argv, init_args, &populated_opts);
	if (ret != 0) {
		return ret;
	}

	state->interactive = isatty(priv->term_fd) &&
		init_args->command_str == NULL && init_args->command_file == NULL;
	if (state->interactive && !(populated_opts & MRSH_OPT_MONITOR)) {
		state->options |= MRSH_OPT_MONITOR;
	}

	return 0;
}
