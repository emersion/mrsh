#define _XOPEN_SOURCE 700
#include <mrsh/builtin.h>
#include <mrsh/entry.h>
#include <mrsh/shell.h>
#include <mrsh/parser.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "builtin.h"
#include "parser.h"
#include "shell/path.h"
#include "shell/trap.h"

static char *expand_str(struct mrsh_state *state, const char *src) {
	struct mrsh_parser *parser = mrsh_parser_with_data(src, strlen(src));
	if (parser == NULL) {
		return NULL;
	}
	struct mrsh_word *word = parameter_expansion_word(parser);
	if (word == NULL) {
		struct mrsh_position err_pos;
		const char *err_msg = mrsh_parser_error(parser, &err_pos);
		if (err_msg != NULL) {
			fprintf(stderr, "%d:%d: syntax error: %s\n",
				err_pos.line, err_pos.column, err_msg);
		} else {
			fprintf(stderr, "expand_str: unknown error\n");
		}
		mrsh_parser_destroy(parser);
		return NULL;
	}
	mrsh_parser_destroy(parser);
	mrsh_run_word(state, &word);
	char *str = mrsh_word_str(word);
	mrsh_word_destroy(word);
	return str;
}

static char *expand_ps(struct mrsh_state *state, const char *name) {
	const char *ps = mrsh_env_get(state, name, NULL);
	if (ps == NULL) {
		return NULL;
	}
	char *str = expand_str(state, ps);
	if (str == NULL) {
		fprintf(stderr, "failed to expand '%s'\n", name);
		// On error, fallback to the default PSn value
	}
	return str;
}

char *mrsh_get_ps1(struct mrsh_state *state, int next_history_id) {
	// TODO: Replace ! with next history ID
	char *str = expand_ps(state, "PS1");
	if (str != NULL) {
		return str;
	}
	char *p = malloc(3);
	sprintf(p, "%s", getuid() ? "$ " : "# ");
	return p;
}

char *mrsh_get_ps2(struct mrsh_state *state) {
	// TODO: Replace ! with next history ID
	char *str = expand_ps(state, "PS2");
	if (str != NULL) {
		return str;
	}
	return strdup("> ");
}

char *mrsh_get_ps4(struct mrsh_state *state) {
	char *str = expand_ps(state, "PS4");
	if (str != NULL) {
		return str;
	}
	return strdup("+ ");
}

bool mrsh_populate_env(struct mrsh_state *state, char **environ) {
	for (size_t i = 0; environ[i] != NULL; ++i) {
		char *eql = strchr(environ[i], '=');
		size_t klen = eql - environ[i];
		char *key = strndup(environ[i], klen);
		char *val = &eql[1];
		mrsh_env_set(state, key, val, MRSH_VAR_ATTRIB_EXPORT);
		free(key);
	}

	mrsh_env_set(state, "IFS", " \t\n", MRSH_VAR_ATTRIB_NONE);

	pid_t ppid = getppid();
	char ppid_str[24];
	snprintf(ppid_str, sizeof(ppid_str), "%d", ppid);
	mrsh_env_set(state, "PPID", ppid_str, MRSH_VAR_ATTRIB_NONE);

	// TODO check if path is well-formed, has . or .., and handle symbolic links
	const char *pwd = mrsh_env_get(state, "PWD", NULL);
	if (pwd == NULL) {
		char *cwd = current_working_dir();
		if (cwd == NULL) {
			perror("current_working_dir failed");
			return false;
		}
		mrsh_env_set(state, "PWD", cwd,
				MRSH_VAR_ATTRIB_EXPORT | MRSH_VAR_ATTRIB_READONLY);
		free(cwd);
	} else {
		mrsh_env_set(state, "PWD", pwd,
				MRSH_VAR_ATTRIB_EXPORT | MRSH_VAR_ATTRIB_READONLY);
	}

	mrsh_env_set(state, "OPTIND", "1", MRSH_VAR_ATTRIB_NONE);
	return true;
}

static void source_file(struct mrsh_state *state, char *path) {
	if (access(path, F_OK) == -1) {
		return;
	}
	char *env_argv[] = { ".", path };
	mrsh_run_builtin(state, sizeof(env_argv) / sizeof(env_argv[0]), env_argv);
}

void mrsh_source_profile(struct mrsh_state *state) {
	source_file(state, "/etc/profile");

	const char *home = getenv("HOME");
	int n = snprintf(NULL, 0, "%s/.profile", home);
	if (n < 0) {
		perror("snprintf failed");
		return;
	}
	char *path = malloc(n + 1);
	if (path == NULL) {
		perror("malloc failed");
		return;
	}
	snprintf(path, n + 1, "%s/.profile", home);

	source_file(state, path);

	free(path);
}

void mrsh_source_env(struct mrsh_state *state) {
	char *path = getenv("ENV");
	if (path == NULL) {
		return;
	}
	if (getuid() != geteuid() || getgid() != getegid()) {
		return;
	}
	path = expand_str(state, path);
	if (path[0] != '/') {
		fprintf(stderr, "Error: $ENV is not an absolute path; "
				"this is undefined behavior.\n");
		fprintf(stderr, "Continuing without sourcing it.\n");
	} else {
		source_file(state, path);
	}
	free(path);
}

bool mrsh_run_exit_trap(struct mrsh_state *state) {
	return run_exit_trap(state);
}
