#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char eval_usage[] = "usage: eval [cmds...]\n";

int builtin_eval(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc == 1) {
		fprintf(stderr, eval_usage);
		return 1;
	}

	struct mrsh_buffer buf = {0};

	for (int i = 1; i < argc; ++i) {
		mrsh_buffer_append(&buf, argv[i], strlen(argv[i]));
		if (i != argc - 1) {
			mrsh_buffer_append_char(&buf, ' ');
		}
	}
	mrsh_buffer_append_char(&buf, '\n');

	struct mrsh_parser *parser = mrsh_parser_with_data(buf.data, buf.len);
	struct mrsh_program *program = mrsh_parse_program(parser);

	int ret;
	if (!program) {
		struct mrsh_position err_pos;
		const char *err_msg = mrsh_parser_error(parser, &err_pos);
		if (err_msg != NULL) {
			fprintf(stderr, "%s %d:%d: %s\n",
				argv[1], err_pos.line, err_pos.column, err_msg);
			ret = 1;
		} else {
			ret = 0;
		}
	} else {
		ret = mrsh_run_program(state, program);
	}

	mrsh_program_destroy(program);
	mrsh_parser_destroy(parser);
	mrsh_buffer_finish(&buf);
	return ret;
}
