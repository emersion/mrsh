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

static const char source_usage[] = "usage: . <path>\n";

int builtin_source(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, source_usage);
		return EXIT_FAILURE;
	}

	const char *path;
	if (strchr(argv[1], '/')) {
		path = argv[1];
	} else {
		// TODO: Implement $PATH resolution internally
		path = argv[1];
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
	if (!program) {
		struct mrsh_position pos;
		fprintf(stderr, "%s %d:%d: %s\n",
				argv[1], pos.line, pos.column,
				mrsh_parser_error(parser, &pos));
		return EXIT_FAILURE;
	}
	return mrsh_run_program(state, program);
}
