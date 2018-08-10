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

// Keep sorted from the longest to the shortest
static const struct symbol operators[] = {
	{ DLESSDASH, "<<-" },
	{ AND_IF, "&&" },
	{ OR_IF, "||" },
	{ DSEMI, ";;" },
	{ DLESS, "<<" },
	{ DGREAT, ">>" },
	{ LESSAND, "<&" },
	{ GREATAND, ">&" },
	{ LESSGREAT, "<>" },
	{ CLOBBER, ">|" },
};

#define OPERATOR_MAX_LEN 3

static const char *keywords[] = {
	"if",
	"then",
	"else",
	"elif",
	"fi",
	"do",
	"done",

	"case",
	"esac",
	"while",
	"until",
	"for",

	"{",
	"}",
	"!",

	"in",
};

struct mrsh_parser {
	FILE *f; // can be NULL

	char *peek;
	size_t peek_len, peek_cap;

	bool has_sym;
	enum symbol_name sym;
	int lineno;

	struct mrsh_array here_documents;
};

#endif
