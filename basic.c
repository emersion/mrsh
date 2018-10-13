/* Basic, strictly POSIX interactive line interface */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mrsh/shell.h>
#include <mrsh/parser.h>
#include "frontend.h"

void interactive_init(struct mrsh_state *state) {
	// no-op
}

size_t interactive_next(struct mrsh_state *state,
		char **line, const char *prompt) {
	fprintf(stderr, "%s", prompt);
	size_t n = 0;
	char *_line;
	ssize_t l = getline(&_line, &n, state->input);
	if (l == -1 && errno) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 0;
	}
	*line = _line;
	return l;
}
