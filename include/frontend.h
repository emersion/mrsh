#ifndef FRONTEND_H
#define FRONTEND_H

#include <stddef.h>
#include <stdio.h>

void interactive_init(struct mrsh_state *state);
size_t interactive_next(struct mrsh_state *state,
		char **restrict line, const char *prompt);

#endif
