#ifndef SHELL_WORD_H
#define SHELL_WORD_H

#include <mrsh/shell.h>

/**
 * Performs tilde expansion. It leaves the word as-is in case of error.
 */
void expand_tilde(struct mrsh_state *state, struct mrsh_word **word_ptr,
	bool assignment);
/**
 * Performs field splitting on `word`, writing fields to `fields`. This should
 * be done after expansions/substitutions.
 */
void split_fields(struct mrsh_array *fields, const struct mrsh_word *word,
	const char *ifs);
void get_fields_str(struct mrsh_array *strs, const struct mrsh_array *fields);
/**
 * Performs pathname expansion on each item in `fields`.
 */
bool expand_pathnames(struct mrsh_array *expanded,
	const struct mrsh_array *fields);


#endif
