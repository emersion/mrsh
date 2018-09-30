#ifndef _MRSH_BUILTIN_H
#define _MRSH_BUILTIN_H

#include <mrsh/shell.h>

bool mrsh_has_builtin(const char *name);
bool mrsh_has_special_builtin(const char *name);
int mrsh_run_builtin(struct mrsh_state *state, int argc, char *argv[]);

int mrsh_process_args(struct mrsh_state *state, int argc, char *argv[]);

struct mrsh_collect_var {
	const char *key, *value;
};

/** Collects and alpha-sorts variables matching attribs. Count will be set to
 * the number of matching variables. You are responsible for freeing the return
 * value when you're done.*/
struct mrsh_collect_var *mrsh_collect_vars(struct mrsh_state *state,
		uint32_t attribs, size_t *count);

#endif
