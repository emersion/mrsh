#ifndef _PARSER_H
#define _PARSER_H

#include <stdio.h>
#include <mrsh/parser.h>

enum symbol_name {
	WORD,
	ASSIGNMENT_WORD,
	NAME,
	NEWLINE,
	IO_NUMBER,

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

	/* The following are the reserved words. */

	If,
	Then,
	Else,
	Elif,
	Fi,
	Do,
	Done,

	Case,
	Esac,
	While,
	Until,
	For,

	/* These are reserved words, not operator tokens, and are
	   recognized when reserved words are recognized. */
	Lbrace,
	Rbrace,
	Bang,

	In,

	/* Special symbols */

	EOF_TOKEN = -1,
	TOKEN = -2,
};

struct symbol {
	enum symbol_name name;
	char *str;
};

static const struct symbol operators[] = {
	{ AND_IF, "&&" },
	{ OR_IF, "||" },
	{ DSEMI, ";;" },
	{ DLESS, "<<" },
	{ DGREAT, ">>" },
	{ LESSAND, "<&" },
	{ GREATAND, ">&" },
	{ LESSGREAT, "<>" },
	{ DLESSDASH, "<<-" },

	{ CLOBBER, ">|" },
};

static const struct symbol keywords[] = {
	{ If, "if" },
	{ Then, "then" },
	{ Else, "else" },
	{ Elif, "elif" },
	{ Fi, "fi" },
	{ Do, "do" },
	{ Done, "done" },

	{ Case, "case" },
	{ Esac, "esac" },
	{ While, "while" },
	{ Until, "until" },
	{ For, "for" },

	{ Lbrace, "{" },
	{ Rbrace, "}" },
	{ Bang, "!" },

	{ In, "in" },
};

struct mrsh_parser {
	FILE *f;

	char *peek;
	size_t peek_len, peek_cap;

	struct symbol sym;
	size_t sym_str_len, sym_str_cap;
};

#endif
