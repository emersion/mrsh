#ifndef _PARSER_H
#define _PARSER_H

#include <stdio.h>
#include <mrsh/parser.h>

enum symbol_name {
	EOF_TOKEN,
	TOKEN,

	NEWLINE,

	/* The following are the operators (see XBD Operator)
	   containing more than one character. */

	AND_IF,
	OR_IF,
	DSEMI,

	DLESS,
	DGREAT,
	LESSAND,
	GREATAND,
	LESSGREAT,
	DLESSDASH,

	CLOBBER,
};

struct symbol {
	enum symbol_name name;
	char *str;
};

extern const struct symbol operators[];
extern const size_t operators_len;
extern const size_t operators_max_str_len;

extern const char *keywords[];
extern const size_t keywords_len;

struct mrsh_parser {
	FILE *f; // can be NULL

	struct buffer buf;
	struct mrsh_position pos;

	struct {
		char *msg;
		struct mrsh_position pos;
	} error;

	bool has_sym;
	enum symbol_name sym;

	struct mrsh_array here_documents;

	mrsh_parser_alias_func_t alias;
	void *alias_user_data;
};

size_t parser_peek(struct mrsh_parser *state, char *buf, size_t size);
char parser_peek_char(struct mrsh_parser *state);
size_t parser_read(struct mrsh_parser *state, char *buf, size_t size);
char parser_read_char(struct mrsh_parser *state);
char *read_token(struct mrsh_parser *state, size_t len);
void parser_set_error(struct mrsh_parser *state, const char *msg);
bool is_operator_start(char c);
enum symbol_name get_symbol(struct mrsh_parser *state);
void consume_symbol(struct mrsh_parser *state);
bool symbol(struct mrsh_parser *state, enum symbol_name sym);
bool eof(struct mrsh_parser *state);
bool newline(struct mrsh_parser *state);
void linebreak(struct mrsh_parser *state);
bool newline_list(struct mrsh_parser *state);

size_t peek_name(struct mrsh_parser *state, bool in_braces);
size_t peek_word(struct mrsh_parser *state, char end);
struct mrsh_word *expect_dollar(struct mrsh_parser *state);
struct mrsh_word *back_quotes(struct mrsh_parser *state);
struct mrsh_word *word(struct mrsh_parser *state, char end);

#endif
