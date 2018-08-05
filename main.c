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

static void print_ps1(struct mrsh_state *state) {
	const char *ps1 = mrsh_hashtable_get(&state->variables, "PS1");
	if (!ps1) {
		ps1 = getuid() ? "$ " : "# ";
	} else {
		// TODO: Run ps1 through shell expansion
		// TODO: Replace ! with next history ID
	}
	fprintf(stderr, "%s", ps1);
}

int main(int argc, char *argv[]) {
	struct mrsh_state state = {0};
	mrsh_state_init(&state);

	if (set(&state, argc, argv, true) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	if (state.interactive) {
		print_ps1(&state);
	}
	struct mrsh_parser *parser = mrsh_parser_create(state.input);
	while (state.exit == -1) {
		struct mrsh_program *prog = mrsh_parse_line(parser);
		if (prog == NULL) {
			state.exit = EXIT_SUCCESS;
			break;
		}
		if ((state.options & MRSH_OPT_NOEXEC)) {
			mrsh_program_print(prog);
		} else {
			mrsh_run_program(&state, prog);
		}
		mrsh_program_destroy(prog);
		if (state.interactive) {
			print_ps1(&state);
		}
	}

	mrsh_parser_destroy(parser);
	mrsh_state_finish(&state);
	fclose(state.input);
	return state.exit;
}
