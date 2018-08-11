#ifndef _MRSH_PARSER_H
#define _MRSH_PARSER_H

#include <mrsh/ast.h>
#include <stdio.h>

struct mrsh_parser;

struct mrsh_program *mrsh_parse(FILE *f);
struct mrsh_parser *mrsh_parser_create(FILE *f);
struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len);
void mrsh_parser_destroy(struct mrsh_parser *state);
struct mrsh_program *mrsh_parse_line(struct mrsh_parser *state);
struct mrsh_word *mrsh_parse_word(struct mrsh_parser *state);
bool mrsh_parser_eof(struct mrsh_parser *state);

#endif
