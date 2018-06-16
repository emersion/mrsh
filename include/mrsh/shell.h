#ifndef _MRSH_SHELL_H
#define _MRSH_SHELL_H

#include <mrsh/ast.h>

struct mrsh_state {};

void mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog);
void mrsh_run_command_list(struct mrsh_state *state,
	struct mrsh_command_list *list);

#endif
