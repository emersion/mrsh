#ifndef _SHELL_SHELL_H
#define _SHELL_SHELL_H

#include <mrsh/shell.h>

struct context {
	struct mrsh_state *state;
	int stdin_fileno;
	int stdout_fileno;
};

void function_destroy(struct mrsh_function *fn);

#endif
