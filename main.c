#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <mrsh/ast.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"

char *expand_ps1(struct mrsh_state *state, const char *ps1) {
	struct mrsh_parser *parser =
		mrsh_parser_create_from_buffer(ps1, strlen(ps1));
	struct mrsh_word *word = mrsh_parse_word(parser);
	mrsh_parser_destroy(parser);
	if (word == NULL) {
		return NULL;
	}

	mrsh_run_word(state, &word);

	return mrsh_word_str(word);
}

static void print_ps1(struct mrsh_state *state) {
	const char *ps1 = mrsh_env_get(state, "PS1", NULL);
	if (ps1 != NULL) {
		char *expanded_ps1 = expand_ps1(state, ps1);
		// TODO: Replace ! with next history ID
		fprintf(stderr, "%s", expanded_ps1);
		free(expanded_ps1);
	} else {
		fprintf(stderr, "%s", getuid() ? "$ " : "# ");
	}

	fflush(stderr);
}

static void source_profile(struct mrsh_state *state) {
	char path[PATH_MAX + 1];
	int n = snprintf(path, sizeof(path), "%s/.profile", getenv("HOME"));
	if (n == sizeof(path)) {
		fprintf(stderr, "Warning: $HOME/.profile is longer than PATH_MAX\n");
		return;
	}
	if (access(path, F_OK) == -1) {
		return;
	}
	char *profile_argv[2] = { ".", path };
	mrsh_run_builtin(state, 2, profile_argv);
}

static const char *get_alias(const char *name, void *data) {
	struct mrsh_state *state = data;
	return mrsh_hashtable_get(&state->aliases, name);
}

extern char **environ;

int main(int argc, char *argv[]) {
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	if (mrsh_process_args(&state, argc, argv) != EXIT_SUCCESS) {
		mrsh_state_finish(&state);
		return EXIT_FAILURE;
	}

	for (size_t i = 0; environ[i] != NULL; ++i) {
		char *eql = strchr(environ[i], '=');
		size_t klen = eql - environ[i];
		char *key = strndup(environ[i], klen);
		char *val = &eql[1];
		mrsh_env_set(&state, key, val, MRSH_VAR_ATTRIB_EXPORT);
		free(key);
	}

	mrsh_env_set(&state, "IFS", " \t\n", MRSH_VAR_ATTRIB_NONE);

	pid_t ppid = getppid();
	char ppid_str[24];
	snprintf(ppid_str, sizeof(ppid_str), "%d", ppid);
	mrsh_env_set(&state, "PPID", ppid_str, MRSH_VAR_ATTRIB_NONE);

	if (state.interactive && !(state.options & MRSH_OPT_NOEXEC)) {
		source_profile(&state);
	}

	// TODO check if path is well-formed, has . or .., and handle symbolic links
	const char *pwd = mrsh_env_get(&state, "PWD", NULL);
	if (pwd == NULL || strlen(pwd) >= PATH_MAX) {
		char cwd[PATH_MAX];
		getcwd(cwd, PATH_MAX);
		mrsh_env_set(&state, "PWD", cwd, MRSH_VAR_ATTRIB_EXPORT);
	}

	struct mrsh_parser *parser = mrsh_parser_create(state.input);
	mrsh_parser_set_alias(parser, get_alias, &state);
	while (state.exit == -1) {
		if (state.interactive) {
			print_ps1(&state);
		}
		struct mrsh_program *prog = mrsh_parse_line(parser);
		if (prog == NULL) {
			struct mrsh_position err_pos;
			const char *err_msg = mrsh_parser_error(parser, &err_pos);
			if (err_msg != NULL) {
				fprintf(stderr, "%s:%d:%d: syntax error: %s\n",
					state.argv[0], err_pos.line, err_pos.column, err_msg);
				if (state.interactive) {
					continue;
				} else {
					state.exit = EXIT_FAILURE;
					break;
				}
			} else if (mrsh_parser_eof(parser)) {
				state.exit = state.last_status;
				break;
			} else {
				fprintf(stderr, "unknown error\n");
				state.exit = EXIT_FAILURE;
				break;
			}
		}
		if ((state.options & MRSH_OPT_NOEXEC)) {
			mrsh_program_print(prog);
		} else {
			mrsh_run_program(&state, prog);
		}
		mrsh_program_destroy(prog);
	}

	if (state.interactive) {
		printf("\n");
	}

	mrsh_parser_destroy(parser);
	mrsh_state_finish(&state);
	fclose(state.input);
	return state.exit;
}
