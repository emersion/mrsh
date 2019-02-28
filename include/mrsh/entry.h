#ifndef MRSH_ENTRY_H
#define MRSH_ENTRY_H

#include <mrsh/shell.h>
#include <stdbool.h>

/**
 * Expands $PS1 or returns the POSIX-specified default of "$" or "#". The caller
 * must free the return value.
 */
char *mrsh_get_ps1(struct mrsh_state *state, int next_history_id);

/**
 * Expands $PS2 or returns the POSIX-specified default of ">". The caller must
 * free the return value.
 */
char *mrsh_get_ps2(struct mrsh_state *state);

/**
 * Expands $PS4 or returns the POSIX-specified default of "+ ". The caller must
 * free the return value.
 */
char *mrsh_get_ps4(struct mrsh_state *state);

/**
 * Copies variables from the environment and sets up internal variables like
 * IFS, PPID, PWD, etc.
 */
bool mrsh_populate_env(struct mrsh_state *state, char **environ);

/**
 * Sources /etc/profile and $HOME/.profile. Note that this behavior is not
 * specified by POSIX. It is recommended to call this file in login shells
 * (for which argv[0][0] == '-' by convention).
 */
void mrsh_source_profile(struct mrsh_state *state);

/** Sources $ENV. It is recommended to source this in interactive shells. */
void mrsh_source_env(struct mrsh_state *state);

#endif
