#ifndef MRSH_BUILTIN_H
#define MRSH_BUILTIN_H

#include <mrsh/shell.h>

bool mrsh_has_builtin(const char *name);
bool mrsh_has_special_builtin(const char *name);
int mrsh_run_builtin(struct mrsh_state *state, int argc, char *argv[]);

struct mrsh_init_args {
	const char *command_file;
	const char *command_str;
};

int mrsh_process_args(struct mrsh_state *state, struct mrsh_init_args *args,
	int argc, char *argv[]);

#endif
