#ifndef SHELL_PATH_H
#define SHELL_PATH_H

#include <mrsh/shell.h>
#include <stdbool.h>

/** Searches $PATH for the requested file and returns it if found. If exec is
 * true, it will require the file to be executable in order to be considered a
 * match. Fully qualified paths are returned as-is. */
const char *expand_path(struct mrsh_state *state, const char *file, bool exec);

const char *expand_exec_path(struct mrsh_state *state, const char *command);
void clear_exec_path_cache(struct mrsh_state *state);

#endif
