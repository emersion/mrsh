#ifndef SHELL_PATH_H
#define SHELL_PATH_H

#include <mrsh/shell.h>
#include <stdbool.h>

/* Searches $PATH for the requested file and returns it if found. If exec is
 * true, it will require the file to be executable in order to be considered a
 * match. If default_path is true, the system's default search path will be
 * used instead of the $PATH variable. Fully qualified paths are returned
 * as-is.
 */
char *expand_path(struct mrsh_state *state, const char *file, bool exec,
	bool default_path);
/* Like getcwd, but returns allocated memory */
char *current_working_dir(void);

#endif
