#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "builtin.h"
#include "shell/path.h"

static const char source_usage[] = "usage: . <path>\n";

int builtin_source(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, source_usage);
		return EXIT_FAILURE;
	}

	const char *path = expand_path(state, argv[1], false);
	if (!path) {
		fprintf(stderr, "%s: not found\n", argv[1]);
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "unable to open %s for reading: %s\n",
				argv[1], strerror(errno));
		if (!state->interactive) {
			state->exit = EXIT_FAILURE;
		}
		return EXIT_FAILURE;
	}

	struct mrsh_parser *parser = mrsh_parser_create(f);
	struct mrsh_program *program = mrsh_parse_program(parser);

	int ret;
	if (!program) {
		struct mrsh_position err_pos;
		const char *err_msg = mrsh_parser_error(parser, &err_pos);
		if (err_msg != NULL) {
			fprintf(stderr, "%s %d:%d: %s\n",
				argv[1], err_pos.line, err_pos.column, err_msg);
			ret = EXIT_FAILURE;
		} else {
			ret = EXIT_SUCCESS;
		}
	} else {
		ret = mrsh_run_program(state, program);
	}

	mrsh_parser_destroy(parser);
	return ret;
}
