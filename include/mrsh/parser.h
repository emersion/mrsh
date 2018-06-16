#ifndef _MRSH_PARSER_H
#define _MRSH_PARSER_H

#include <mrsh/ast.h>
#include <stdio.h>

struct mrsh_parser;

struct mrsh_program *mrsh_parse(FILE *f);
struct mrsh_parser *mrsh_parser_create(FILE *f);
void mrsh_parser_destroy(struct mrsh_parser *state);
struct mrsh_command_list *mrsh_parse_command_list(struct mrsh_parser *state);

#endif
