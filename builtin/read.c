#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mrsh/buffer.h>
#include <mrsh/shell.h>
#include "builtin.h"

// usage: read [-r] var...
static const char read_usage[] = "usage: read [-r] var...\n";

int builtin_read(struct mrsh_state *state, int argc, char *argv[]) {
	bool raw = false;
	
	int opt;
	while ((opt = getopt(argc, argv, "r")) != -1) {
		switch (opt) {
		case 'r':
			raw = true;
			break;
		default:
			fprintf(stderr, read_usage);
			return EXIT_FAILURE;
		}
	}
	if (optind >= argc) {
		fprintf(stderr, read_usage);
		return EXIT_FAILURE;
	}

	// TODO handle multiple variables and split
	if (argc > optind + 1) {
		fprintf(stderr, "read: multiple variables not yet implemented\n");
		return EXIT_FAILURE;
	}
	
	struct mrsh_buffer buf = {0};
	bool escaped = false;
	int c;
	while ((c = fgetc(stdin)) != EOF) {
		if (!raw && !escaped && c == '\\') {
			escaped = true;
			continue;
		}
		if (c == '\n') {
			if (escaped) {
				escaped = false;
				continue;
			}
			break;
		}
		escaped = false;
		mrsh_buffer_append_char(&buf, (char)c);
	}
	mrsh_buffer_append_char(&buf, '\0');

	mrsh_env_set(state, argv[optind], buf.data, MRSH_VAR_ATTRIB_NONE);
	mrsh_buffer_finish(&buf);
	return EXIT_SUCCESS;
}
