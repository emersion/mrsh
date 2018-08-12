#ifndef _MRSH_PARSER_H
#define _MRSH_PARSER_H

#include <mrsh/ast.h>
#include <stdio.h>

struct mrsh_parser;

typedef const char *(*mrsh_parser_alias_func_t)(const char *name,
	void *user_data);

struct mrsh_parser *mrsh_parser_create(FILE *f);
struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len);
void mrsh_parser_destroy(struct mrsh_parser *state);
struct mrsh_program *mrsh_parse_program(struct mrsh_parser *state);
struct mrsh_program *mrsh_parse_line(struct mrsh_parser *state);
struct mrsh_word *mrsh_parse_word(struct mrsh_parser *state);
bool mrsh_parser_eof(struct mrsh_parser *state);
void mrsh_parser_set_alias(struct mrsh_parser *state,
	mrsh_parser_alias_func_t alias, void *user_data);

#endif
