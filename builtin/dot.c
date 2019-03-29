#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mrsh/builtin.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"
#include "shell/path.h"

static const char source_usage[] = "usage: . <path>\n";

int builtin_dot(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, source_usage);
		return 1;
	}

	const char *path = expand_path(state, argv[1], false);
	if (!path) {
		fprintf(stderr, "%s: not found\n", argv[1]);
		if (!state->interactive) {
			state->exit = 1;
		}
		return 1;
	}

	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "unable to open %s for reading: %s\n",
				argv[1], strerror(errno));
		goto error;
	}

	struct mrsh_parser *parser = mrsh_parser_with_fd(fd);
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
	close(fd);
	return ret;

error:
	if (!state->interactive) {
		state->exit = 1;
	}
	return 1;
}
