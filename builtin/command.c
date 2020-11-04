#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "parser.h"
#include "shell/job.h"
#include "shell/path.h"
#include "shell/process.h"
#include "shell/shell.h"

static const char command_usage[] = "usage: command [-v|-V|-p] "
	"command_name [argument...]\n";

static int verify_command(struct mrsh_state *state, const char *command_name,
		bool default_path) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	size_t len_command_name = strlen(command_name);

	const char *look_alias =
		mrsh_hashtable_get(&priv->aliases, command_name);
	if (look_alias != NULL) {
		printf("alias %s='%s'\n", command_name, look_alias);
		return 0;
	}

	const char *look_fn =
		mrsh_hashtable_get(&priv->functions, command_name);
	if (look_fn != NULL) {
		printf("%s\n", command_name);
		return 0;
	}

	if (mrsh_has_builtin(command_name)) {
		printf("%s\n", command_name);
		return 0;
	}

	for (size_t i = 0; i < keywords_len; ++i) {
		if (strlen(keywords[i]) == len_command_name &&
				strcmp(command_name, keywords[i]) == 0) {
			printf("%s\n", command_name);
			return 0;
		}
	}

	char *expanded = expand_path(state, command_name, true, default_path);
	if (expanded != NULL) {
		printf("%s\n", expanded);
		free(expanded);
		return 0;
	}

	return 1;
}

static int run_command(struct mrsh_state *state, int argc, char *argv[],
		bool default_path) {
	if (mrsh_has_builtin(argv[0])) {
		return mrsh_run_builtin(state, argc - _mrsh_optind, &argv[_mrsh_optind]);
	}

	char *path = expand_path(state, argv[0], true, default_path);
	if (path == NULL) {
		fprintf(stderr, "%s: not found\n", argv[0]);
		return 127;
	}

	// TODO: job control support
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return 126;
	} else if (pid == 0) {
		execv(path, argv);

		// Something went wrong
		perror(argv[0]);
		exit(126);
	}

	free(path);

	struct mrsh_process *proc = process_create(state, pid);
	return job_wait_process(proc);
}

int builtin_command(struct mrsh_state *state, int argc, char *argv[]) {
	_mrsh_optind = 0;
	int opt;

	bool verify = false, default_path = false;
	while ((opt = _mrsh_getopt(argc, argv, ":vVp")) != -1) {
		switch (opt) {
		case 'v':
			verify = true;
			break;
		case 'V':
			fprintf(stderr, "command: `-V` has an unspecified output format, "
				"use `-v` instead\n");
			return 0;
		case 'p':
			default_path = true;
			break;
		default:
			fprintf(stderr, "command: unknown option -- %c\n", _mrsh_optopt);
			fprintf(stderr, command_usage);
			return 1;
		}
	}

	if (_mrsh_optind >= argc) {
		fprintf(stderr, command_usage);
		return 1;
	}

	if (verify) {
		if (_mrsh_optind != argc - 1) {
			fprintf(stderr, command_usage);
			return 1;
		}
		return verify_command(state, argv[_mrsh_optind], default_path);
	}

	return run_command(state, argc - _mrsh_optind, &argv[_mrsh_optind],
		default_path);
}
