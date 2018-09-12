#ifndef _SHELL_WORD_H
#define _SHELL_WORD_H

#include <mrsh/shell.h>

enum tilde_expansion {
	// Don't perform tilde expansion at all
	TILDE_EXPANSION_NONE,
	// Only expand at the begining of words
	TILDE_EXPANSION_NAME,
	// Expand at the begining of words and after semicolons
	TILDE_EXPANSION_ASSIGNMENT,
};

/**
 * Performs tilde expansion. It leaves the string as-is in case of error.
 */
void expand_tilde(struct mrsh_state *state, char **str_ptr);
/**
 * Performs field splitting on `word`, writing fields to `fields`. This should
 * be done after expansions/substitutions.
 */
void split_fields(struct mrsh_array *fields, struct mrsh_word *word,
	const char *ifs);
/**
 * Performs pathname expansion on each item in `fields`.
 */
bool expand_pathnames(struct mrsh_array *expanded, struct mrsh_array *fields);


#endif
