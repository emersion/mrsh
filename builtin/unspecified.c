#include <mrsh/shell.h>
#include <stdlib.h>
#include <stdio.h>
#include "builtin.h"

int builtin_unspecified(struct mrsh_state *state, int argc, char *argv[]) {
	// Ref: http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_09_01_01
	if (state->interactive) {
		fprintf(stderr,
				"%s: The behavior of this command is undefined.", argv[0]);
	} else {
		fprintf(stderr, "%s: The behavior of this command is undefined. "
				"This is an error in your script. Aborting.\n", argv[0]);
		state->exit = 1;
	}

	return 1;
}
