#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"
#include "builtin.h"

static const char eval_usage[] = "usage: eval [cmds...]\n";

int builtin_eval(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc == 1) {
		fprintf(stderr, eval_usage);
		return EXIT_FAILURE;
	}

	struct buffer buf = {0};

	for (int i = 1; i < argc; ++i) {
		if (!buffer_reserve(&buf, strlen(argv[i]) + 1)) {
			fprintf(stderr, "Failed to expand parse buffer");
			return EXIT_FAILURE;
		}
		buffer_append(&buf, argv[i], strlen(argv[i]));
		if (i != argc - 1) {
			buffer_append_char(&buf, ' ');
		}
	}
	buffer_append_char(&buf, '\n');

	struct mrsh_parser *parser =
		mrsh_parser_create_from_buffer(buf.data, buf.len);
	struct mrsh_program *program = mrsh_parse_program(parser);
	buffer_finish(&buf);
	if (!program) {
		struct mrsh_position pos;
		fprintf(stderr, "%s %d:%d: %s\n",
				argv[1], pos.line, pos.column,
				mrsh_parser_error(parser, &pos));
		return EXIT_FAILURE;
	}
	return mrsh_run_program(state, program);
}
