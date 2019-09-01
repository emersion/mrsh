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
	size_t len = 0;
	char *_line = NULL;
	errno = 0;
	ssize_t n_read = getline(&_line, &len, stdin);
	if (n_read < 0) {
		free(_line);
		if (errno != 0) {
			perror("getline");
		}
		return 0;
	}
	*line = _line;
	return n_read;
}
