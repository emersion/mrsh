#include <mrsh/shell.h>
#include <stdlib.h>
#include "builtin.h"

int builtin_true(struct mrsh_state *state, int argc, char *argv[]) {
	return EXIT_SUCCESS;
}
