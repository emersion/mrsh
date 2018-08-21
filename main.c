#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/ast.h>
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
	const char *ps1 = mrsh_hashtable_get(&state->variables, "PS1");
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

static const char *get_alias(const char *name, void *data) {
	struct mrsh_state *state = data;
	return mrsh_hashtable_get(&state->aliases, name);
}

extern char **environ;

int main(int argc, char *argv[]) {
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	if (set(&state, argc, argv, true) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	size_t i = 0;
	while (environ[i]) {
		char *eql = strchr(environ[i], '=');
		size_t klen = eql - environ[i] + 1;
		char *key = malloc(klen);
		char *val = malloc(strlen(&eql[1]) + 1);
		strncpy(key, environ[i], klen - 1);
		key[klen - 1] = 0;
		strcpy(val, &eql[1]);
		mrsh_hashtable_set(&state.variables, key, val);
		free(key);
		++i;
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
				state.exit = EXIT_SUCCESS;
				break;
			} else {
				continue;
			}
		}
		if ((state.options & MRSH_OPT_NOEXEC)) {
			mrsh_program_print(prog);
		} else {
			mrsh_run_program(&state, prog);
		}
		mrsh_program_destroy(prog);
	}

	mrsh_parser_destroy(parser);
	mrsh_state_finish(&state);
	fclose(state.input);
	return state.exit;
}
