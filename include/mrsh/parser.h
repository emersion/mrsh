#ifndef MRSH_PARSER_H
#define MRSH_PARSER_H

#include <mrsh/ast.h>
#include <stdio.h>

struct mrsh_parser;

/**
 * An alias callback. The alias named is given as a parameter and the alias
 * value should be returned. NULL should be returned if the alias doesn't exist.
 */
typedef const char *(*mrsh_parser_alias_func_t)(const char *name,
	void *user_data);

struct mrsh_parser *mrsh_parser_create(FILE *f);
struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len);
void mrsh_parser_destroy(struct mrsh_parser *state);
/**
 * Parse a complete multi-line program.
 */
struct mrsh_program *mrsh_parse_program(struct mrsh_parser *state);
/**
 * Parse a program line. Continuation lines are consumed.
 */
struct mrsh_program *mrsh_parse_line(struct mrsh_parser *state);
/**
 * Parse a single word. Continuation lines are consumed.
 */
struct mrsh_word *mrsh_parse_word(struct mrsh_parser *state);

/**
 * Parse an arithmetic expression.
 */
struct mrsh_arithm_expr *mrsh_parse_arithm_expr(struct mrsh_parser *state);
/**
 * Check if the input has been completely consumed.
 */
bool mrsh_parser_eof(struct mrsh_parser *state);
/**
 * Set the alias callback.
 */
void mrsh_parser_set_alias(struct mrsh_parser *state,
	mrsh_parser_alias_func_t alias, void *user_data);
/**
 * Check if the parser ended with a syntax error. The error message is returned.
 * The error position can optionally be obtained.
 */
const char *mrsh_parser_error(struct mrsh_parser *state,
	struct mrsh_position *pos);
/**
 * Check if the input ends on a continuation line.
 */
bool mrsh_parser_continuation_line(struct mrsh_parser *state);

#endif
