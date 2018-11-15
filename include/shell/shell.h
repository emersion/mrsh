#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <mrsh/shell.h>

struct context {
	struct mrsh_state *state;
	int stdin_fileno;
	int stdout_fileno;
};

#endif
