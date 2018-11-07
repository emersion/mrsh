#ifndef SHELL_PATH_H
#define SHELL_PATH_H

#include <mrsh/shell.h>
#include <stdbool.h>

/** Searches $PATH for the requested file and returns it if found. If exec is
 * true, it will require the file to be executable in order to be considered a
 * match. Fully qualified paths are returned as-is. */
const char *expand_path(struct mrsh_state *state, const char *file, bool exec);

#endif
