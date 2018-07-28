#ifndef _MRSH_BUILTIN_H
#define _MRSH_BUILTIN_H

#include <mrsh/shell.h>

int mrsh_has_builtin(const char *name);
int mrsh_run_builtin(struct mrsh_state *state, int argc, char **argv);

#endif
