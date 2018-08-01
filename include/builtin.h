#ifndef _BUILTIN_H
#define _BUILTIN_H

typedef int (*mrsh_builtin_func_t)(struct mrsh_state *state,
	int argc, char *argv[]);

int builtin_exit(struct mrsh_state *state, int argc, char *argv[]);

#endif
