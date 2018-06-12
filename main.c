#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char *symbol_str(enum symbol_name sym) {
	for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
		if (sym == operators[i].name) {
			return operators[i].str;
		}
	}

	for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i) {
		if (sym == keywords[i].name) {
			return keywords[i].str;
		}
	}

	switch (sym) {
	case WORD: return "WORD";
	case ASSIGNMENT_WORD: return "ASSIGNMENT_WORD";
	case NAME: return "NAME";
	case NEWLINE: return "NEWLINE";
	case IO_NUMBER: return "IO_NUMBER";

	case EOF_TOKEN: return "EOF_TOKEN";
	case TOKEN: return "TOKEN";

	default: break;
	}

	return NULL;
}

struct parser_state {
	FILE *f;

	char *peek;
	size_t peek_len, peek_cap;

	struct symbol sym;
	size_t sym_str_len, sym_str_cap;
};

static void parser_init(struct parser_state *state, FILE *f) {
	state->f = f;

	state->peek_cap = 128;
	state->peek = malloc(state->peek_cap);
	state->peek_len = 0;

	state->sym_str_cap = 128;
	state->sym.str = malloc(state->sym_str_cap);
	state->sym.str[0] = '\0';
	state->sym_str_len = 0;
}

static size_t parser_peek(struct parser_state *state, char *buf, size_t size) {
	if (size > state->peek_len) {
		if (size > state->peek_cap) {
			state->peek = realloc(state->peek, size);
			if (state->peek == NULL) {
				state->peek_cap = 0;
				return 0;
			}
			state->peek_cap = size;
		}

		size_t n_more = size - state->peek_len;
		size_t n_read =
			fread(state->peek + state->peek_len, 1, n_more, state->f);
		state->peek_len += n_read;
		if (n_read < n_more) {
			if (feof(state->f)) {
				state->peek[state->peek_len] = '\0';
				state->peek_len++;
				size = state->peek_len;
			} else {
				return 0;
			}
		}
	}

	if (buf != NULL) {
		memcpy(buf, state->peek, size);
	}
	return size;
}

static char parser_peek_char(struct parser_state *state) {
	char c = '\0';
	parser_peek(state, &c, sizeof(char));
	return c;
}

static size_t parser_read(struct parser_state *state, char *buf, size_t size) {
	size_t n = parser_peek(state, buf, size);
	if (n > 0) {
		memmove(state->peek, state->peek + n, state->peek_len - n);
		state->peek_len -= n;
	}
	return n;
}

static char parser_read_char(struct parser_state *state) {
	char c = '\0';
	parser_read(state, &c, sizeof(char));
	return c;
}

static bool accept_char(struct parser_state *state, char c) {
	if (parser_peek_char(state) == c) {
		parser_read_char(state);
		return true;
	}
	return false;
}

static bool accept_str(struct parser_state *state, const char *str) {
	size_t len = strlen(str);

	char next[len];
	size_t n_read = parser_peek(state, next, len);
	if (n_read == len && strncmp(next, str, len) == 0) {
		parser_read(state, next, len);
		return true;
	}
	return false;
}

static void parser_sym_reset(struct parser_state *state) {
	if (state->sym_str_cap > 0) {
		state->sym.str[0] = '\0';
	}
	state->sym_str_len = 0;
}

static void parser_sym_begin(struct parser_state *state, enum symbol_name sym) {
	parser_sym_reset(state);
	state->sym.name = sym;
}

static void parser_sym_append_char(struct parser_state *state, char c) {
	size_t min_cap = state->sym_str_len + 2; // new char + NULL char
	if (min_cap > state->sym_str_cap) {
		size_t new_cap = state->sym_str_cap * 2;
		if (new_cap < min_cap) {
			new_cap = min_cap;
		}
		state->sym.str = realloc(state->sym.str, new_cap);
		if (state->sym.str == NULL) {
			state->sym_str_cap = 0;
			return;
		}
		state->sym_str_cap = new_cap;
	}

	state->sym.str[state->sym_str_len] = c;
	state->sym.str[state->sym_str_len + 1] = '\0';
	state->sym_str_len++;
}

static bool is_operator_start(char c) {
	switch (c) {
	case '&':
	case '|':
	case ';':
	case '<':
	case '>':
		return true;
	default:
		return false;
	}
}

static void single_quotes(struct parser_state *state) {
	char c = parser_read_char(state);
	assert(c == '\'');

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "single quotes not terminated\n");
			exit(EXIT_FAILURE);
		}
		if (c == '\'') {
			parser_read_char(state);
			break;
		}

		parser_sym_append_char(state, c);
		parser_read_char(state);
	}
}

static void double_quotes(struct parser_state *state) {
	char c = parser_read_char(state);
	assert(c == '"');

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "double quotes not terminated\n");
			exit(EXIT_FAILURE);
		}
		if (c == '"') {
			parser_read_char(state);
			break;
		}

		if (c == '$' || c == '`') {
			// TODO
			fprintf(stderr, "not yet implemented\n");
			exit(EXIT_FAILURE);
		}

		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(state, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '"':
			case '\\':
				parser_read_char(state);
				c = next[1];
				break;
			}

			if (next[1] == '\n') {
				parser_read(state, NULL, 2 * sizeof(char));
				continue;
			}
		}

		parser_sym_append_char(state, c);
		parser_read_char(state);
	}
}

static void word(struct parser_state *state) {
	bool first = true;
	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0' || c == '\n') {
			break;
		}

		if (c == '$' || c == '`') {
			// TODO
			fprintf(stderr, "not yet implemented\n");
			exit(EXIT_FAILURE);
		}

		// Quoting
		if (c == '\'') {
			single_quotes(state);
			continue;
		}
		if (c == '"') {
			double_quotes(state);
			continue;
		}
		if (c == '\\') {
			// Unquoted backslash
			parser_read_char(state);
			c = parser_peek_char(state);
			if (c == '\n') {
				// Continuation line
				parser_read_char(state);
				continue;
			}
		}

		if (!first && is_operator_start(c)) {
			break;
		}
		if (isblank(c)) {
			break;
		}

		parser_sym_append_char(state, c);
		parser_read_char(state);
		first = false;
	}
}

// See section 2.3 Token Recognition
static void token(struct parser_state *state) {
	char c = parser_peek_char(state);

	if (accept_char(state, '\0')) {
		parser_sym_begin(state, EOF_TOKEN);
		return;
	}
	if (accept_char(state, '\n')) {
		parser_sym_begin(state, NEWLINE);
		return;
	}

	if (is_operator_start(c)) {
		for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
			if (accept_str(state, operators[i].str)) {
				parser_sym_begin(state, operators[i].name);
				return;
			}
		}

		parser_sym_begin(state, TOKEN);
		parser_sym_append_char(state, c);
		parser_read_char(state);
		return;
	}

	if (isblank(c)) {
		parser_read_char(state);
		token(state);
		return;
	}

	if (accept_char(state, '#')) {
		while (true) {
			char c = parser_peek_char(state);
			if (c == '\0' || c == '\n') {
				break;
			}
			parser_read_char(state);
		}
		token(state);
		return;
	}

	parser_sym_begin(state, TOKEN);
	word(state);
}

static void symbol(struct parser_state *state) {
	token(state);

	if (state->sym.name == TOKEN) {
		char next = parser_peek_char(state);
		if (strlen(state->sym.str) == 1 && isdigit(state->sym.str[0]) &&
				(next == '<' || next == '>')) {
			state->sym.name = IO_NUMBER;
		}
	}
}

static void next_sym(struct parser_state *state) {
	parser_sym_reset(state);
	symbol(state);
	if (state->sym.name == EOF_TOKEN) {
		return;
	}

	fprintf(stderr, "symbol: %s \"%s\"\n",
		symbol_str(state->sym.name), state->sym.str);
}

static bool accept(struct parser_state *state, enum symbol_name sym) {
	if (state->sym.name == sym) {
		next_sym(state);
		return true;
	}
	return false;
}

static void expect(struct parser_state *state, enum symbol_name sym) {
	if (accept(state, sym)) {
		return;
	}
	fprintf(stderr, "unexpected symbol: expected %s, got %s\n",
		symbol_str(sym), symbol_str(state->sym.name));
	exit(EXIT_FAILURE);
}

static bool accept_token(struct parser_state *state, const char *str) {
	return strcmp(str, state->sym.str) == 0 && accept(state, TOKEN);
}

static bool newline_list(struct parser_state *state) {
	if (!accept(state, NEWLINE)) {
		return false;
	}

	while (accept(state, NEWLINE)) {
		// This space is intentionally left blank
	}
	return true;
}

static void linebreak(struct parser_state *state) {
	newline_list(state);
}

static bool separator_op(struct parser_state *state) {
	return accept_token(state, "&") || accept_token(state, ";");
}

static bool io_file(struct parser_state *state) {
	return false; // TODO
}

static bool filename(struct parser_state *state) {
	// TODO: Apply rule 2
	return accept(state, TOKEN);
}

static bool io_here(struct parser_state *state) {
	return (accept_token(state, "<")
			|| accept(state, LESSAND)
			|| accept_token(state, ">")
			|| accept(state, GREATAND)
			|| accept(state, DGREAT)
			|| accept(state, CLOBBER)
			|| accept(state, LESSGREAT))
		&& filename(state);
}

static bool io_redirect(struct parser_state *state) {
	if (accept(state, IO_NUMBER)) {
		// TODO
	}

	return io_file(state) || io_here(state);
}

static bool cmd_prefix(struct parser_state *state) {
	if (!io_redirect(state) && !accept(state, ASSIGNMENT_WORD)) {
		return false;
	}

	while (io_redirect(state) || accept(state, ASSIGNMENT_WORD)) {
		// This space is intentionally left blank
	}
	return true;
}

static void rule1(struct parser_state *state) {
	// Apply rule 1
	assert(state->sym.name == TOKEN);

	for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i) {
		if (strcmp(state->sym.str, keywords[i].str) == 0) {
			state->sym.name = keywords[i].name;
			return;
		}
	}

	state->sym.name = WORD;
}

static bool cmd_word(struct parser_state *state) {
	// Apply rule 7b
	// TODO: handle quotes

	//const char *pos = strchr(state->sym.str, '=');
	// TODO: handle pos

	rule1(state);
	fprintf(stderr, "cmd_word: %s\n", state->sym.str);
	next_sym(state);
	return true;
}

static void cmd_name(struct parser_state *state) {
	// Apply rule 7a
	if (strchr(state->sym.str, '=') == NULL) {
		// Apply rule 1
		rule1(state);
		fprintf(stderr, "cmd_name: %s\n", state->sym.str);
		next_sym(state);
	} else {
		// Apply rule 7b
		if (!cmd_word(state)) {
			fprintf(stderr, "expected a cmd_word");
			exit(EXIT_FAILURE);
		}

		fprintf(stderr, "cmd_name\n");
	}
}

static bool cmd_suffix(struct parser_state *state) {
	// TODO: s/TOKEN/WORD/, with rule 1?
	if (!io_redirect(state) && !accept(state, TOKEN)) {
		return false;
	}

	while (io_redirect(state) || accept(state, TOKEN)) {
		// This space is intentionally left blank
	}
	return true;
}

static void simple_command(struct parser_state *state) {
	if (cmd_prefix(state)) {
		if (cmd_word(state)) {
			if (cmd_suffix(state)) {
				// TODO
			}
		}
	} else {
		cmd_name(state);
		if (cmd_suffix(state)) {
			// TODO
		}
	}
}

static void command(struct parser_state *state) {
	// TODO: compound_command
	// TODO: compound_command redirect_list
	// TODO: function_definition

	simple_command(state);
}

static void pipeline(struct parser_state *state) {
	if (accept(state, Bang)) {
		// TODO
	}

	command(state);
	while (accept_token(state, "|")) {
		linebreak(state);
		command(state);
	}
}

static void and_or(struct parser_state *state) {
	pipeline(state);

	while (accept(state, AND_IF) || accept(state, OR_IF)) {
		linebreak(state);
		and_or(state);
	}
}

static void list(struct parser_state *state) {
	and_or(state);

	while (separator_op(state)) {
		and_or(state);
	}
}

static void complete_command(struct parser_state *state) {
	list(state);
	if (separator_op(state)) {
		// TODO
	}
}

static void program(struct parser_state *state) {
	linebreak(state);
	if (accept(state, EOF_TOKEN)) {
		return;
	}

	complete_command(state);

	while (newline_list(state) && state->sym.name != EOF_TOKEN) {
		complete_command(state);
	}

	linebreak(state);
	expect(state, EOF_TOKEN);
}

int main(int argc, char *argv[]) {
	struct parser_state state = {0};
	parser_init(&state, stdin);
	next_sym(&state);
	program(&state);
	return 0;
}
