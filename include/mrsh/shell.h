#ifndef _MRSH_SHELL_H
#define _MRSH_SHELL_H

#include <mrsh/ast.h>

struct mrsh_state {
	int exit;
};

void mrsh_state_init(struct mrsh_state *state);
int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog);

#endif
