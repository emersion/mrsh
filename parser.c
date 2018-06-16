#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "ast.h"
#include "parser.h"

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

static char *take(struct parser_state *state, enum symbol_name sym) {
	if (state->sym.name != sym) {
		return NULL;
	}
	char *str = strdup(state->sym.str);
	next_sym(state);
	return str;
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

static int separator_op(struct parser_state *state) {
	if (accept_token(state, "&")) {
		return '&';
	}
	if (accept_token(state, ";")) {
		return ';';
	}
	return -1;
}

static bool io_here(struct parser_state *state) {
	return false; // TODO
}

static char *filename(struct parser_state *state) {
	// TODO: Apply rule 2
	return take(state, TOKEN);
}

static bool io_file(struct parser_state *state,
		struct mrsh_io_redirect *redir) {
	char *str = strdup(state->sym.str);
	enum symbol_name name = state->sym.name;
	if (accept_token(state, "<") || accept_token(state, ">")) {
		redir->op = str;
	} else if (accept(state, LESSAND)
			|| accept(state, GREATAND)
			|| accept(state, DGREAT)
			|| accept(state, CLOBBER)
			|| accept(state, LESSGREAT)) {
		redir->op = strdup(symbol_str(name));
		free(str);
	} else {
		return false;
	}

	redir->filename = filename(state);
	return (redir->filename != NULL);
}

static struct mrsh_io_redirect *io_redirect(struct parser_state *state) {
	struct mrsh_io_redirect redir = {
		.io_number = -1,
	};

	if (state->sym.name == IO_NUMBER) {
		redir.io_number = strtol(state->sym.str, NULL, 10);
		accept(state, IO_NUMBER);
	}

	if (io_file(state, &redir)) {
		struct mrsh_io_redirect *redir_ptr =
			calloc(1, sizeof(struct mrsh_io_redirect));
		memcpy(redir_ptr, &redir, sizeof(struct mrsh_io_redirect));
		return redir_ptr;
	}

	if (io_here(state)) {
		return NULL; // TODO
	}

	return NULL;
}

static bool cmd_prefix(struct parser_state *state, struct mrsh_simple_command *cmd) {
	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	char *assign = take(state, ASSIGNMENT_WORD);
	if (assign != NULL) {
		mrsh_array_add(&cmd->assignments, strdup(state->sym.str));
		return true;
	}

	return false;
}

static void apply_rule1(struct parser_state *state) {
	if (state->sym.name != TOKEN) {
		return;
	}

	for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i) {
		if (strcmp(state->sym.str, keywords[i].str) == 0) {
			state->sym.name = keywords[i].name;
			return;
		}
	}

	state->sym.name = WORD;
}

static void apply_rule7b(struct parser_state *state) {
	if (state->sym.name != TOKEN && state->sym.name != WORD) {
		return;
	}

	// TODO: handle quotes
	const char *pos = strchr(state->sym.str, '=');
	if (pos != NULL && pos != state->sym.str) {
		// TODO: check that chars before = form a valid name
		state->sym.name = ASSIGNMENT_WORD;
		return;
	}

	apply_rule1(state);
}

static void apply_rule7a(struct parser_state *state) {
	if (state->sym.name != TOKEN && state->sym.name != WORD) {
		return;
	}

	if (strchr(state->sym.str, '=') == NULL) {
		apply_rule1(state);
	} else {
		apply_rule7b(state);
	}
}

static bool cmd_suffix(struct parser_state *state, struct mrsh_simple_command *cmd) {
	// TODO
	if (strcmp(state->sym.str, "|") == 0 ||
			strcmp(state->sym.str, "&") == 0 ||
			strcmp(state->sym.str, ";") == 0) {
		return false;
	}

	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	// TODO: s/TOKEN/WORD/, with rule 1?
	char *arg = take(state, TOKEN);
	if (arg != NULL) {
		mrsh_array_add(&cmd->arguments, arg);
		return true;
	}

	return false;
}

static struct mrsh_simple_command *simple_command(struct parser_state *state) {
	struct mrsh_simple_command cmd = {0};

	apply_rule7a(state);

	while (cmd_prefix(state, &cmd)) {
		apply_rule7b(state);
	}

	cmd.name = take(state, WORD);
	if (cmd.name == NULL) {
		return NULL;
	}

	while (cmd_suffix(state, &cmd)) {
		// This space is intentionally left blank
	}

	return mrsh_simple_command_create(cmd.name, &cmd.arguments,
		&cmd.io_redirects, &cmd.assignments);
}

static int separator(struct parser_state *state) {
	int sep = separator_op(state);
	if (sep != -1) {
		linebreak(state);
		return sep;
	}

	if (newline_list(state)) {
		return '\n';
	}

	return -1;
}

static struct mrsh_node *and_or(struct parser_state *state);

static struct mrsh_command_list *term(struct parser_state *state) {
	struct mrsh_node *node = and_or(state);
	if (node == NULL) {
		return NULL;
	}

	struct mrsh_command_list *cmd = calloc(1, sizeof(struct mrsh_command_list));
	cmd->node = node;

	int sep = separator(state);
	if (sep == '&') {
		cmd->ampersand = true;
	}

	return cmd;
}

static void compound_list(struct parser_state *state, struct mrsh_array *cmds) {
	linebreak(state);

	struct mrsh_command_list *l = term(state);
	assert(l != NULL);
	mrsh_array_add(cmds, l);

	while (true) {
		l = term(state);
		if (l == NULL) {
			break;
		}
		mrsh_array_add(cmds, l);
	}
}

static struct mrsh_brace_group *brace_group(struct parser_state *state) {
	apply_rule1(state);
	if (!accept(state, Lbrace)) {
		return NULL;
	}

	struct mrsh_array body = {0};
	compound_list(state, &body);

	apply_rule1(state);
	expect(state, Rbrace);
	return mrsh_brace_group_create(&body);
}

static struct mrsh_command *else_part(struct parser_state *state) {
	apply_rule1(state);

	if (accept(state, Elif)) {
		struct mrsh_array cond = {0};
		compound_list(state, &cond);

		apply_rule1(state);
		expect(state, Then);

		struct mrsh_array body = {0};
		compound_list(state, &body);

		struct mrsh_command *ep = else_part(state);

		struct mrsh_if_clause *ic = mrsh_if_clause_create(&cond, &body, ep);
		return &ic->command;
	}

	if (accept(state, Else)) {
		struct mrsh_array body = {0};
		compound_list(state, &body);

		struct mrsh_brace_group *bg = mrsh_brace_group_create(&body);
		return &bg->command;
	}

	return NULL;
}

static struct mrsh_if_clause *if_clause(struct parser_state *state) {
	apply_rule1(state);
	if (!accept(state, If)) {
		return NULL;
	}

	struct mrsh_array cond = {0};
	compound_list(state, &cond);

	apply_rule1(state);
	expect(state, Then);

	struct mrsh_array body = {0};
	compound_list(state, &body);

	struct mrsh_command *ep = else_part(state);

	apply_rule1(state);
	expect(state, Fi);

	return mrsh_if_clause_create(&cond, &body, ep);
}

static struct mrsh_command *command(struct parser_state *state) {
	struct mrsh_simple_command *sc = simple_command(state);
	if (sc) {
		return &sc->command;
	}

	struct mrsh_brace_group *bg = brace_group(state);
	if (bg) {
		return &bg->command;
	}

	struct mrsh_if_clause *ic = if_clause(state);
	if (ic) {
		return &ic->command;
	}

	// TODO: subshell for_clause case_clause while_clause until_clause
	// TODO: compound_command redirect_list
	// TODO: function_definition
	return NULL;
}

static struct mrsh_pipeline *pipeline(struct parser_state *state) {
	apply_rule1(state);
	bool bang = accept(state, Bang);

	struct mrsh_command *cmd = command(state);
	if (cmd == NULL) {
		return NULL;
	}

	struct mrsh_array commands = {0};
	mrsh_array_add(&commands, cmd);

	while (accept_token(state, "|")) {
		linebreak(state);
		struct mrsh_command *cmd = command(state);
		mrsh_array_add(&commands, cmd);
	}

	return mrsh_pipeline_create(&commands, bang);
}

static struct mrsh_node *and_or(struct parser_state *state) {
	struct mrsh_pipeline *pl = pipeline(state);
	if (pl == NULL) {
		return NULL;
	}

	int binop_type = -1;
	if (accept(state, AND_IF)) {
		binop_type = MRSH_BINOP_AND;
	} else if (accept(state, OR_IF)) {
		binop_type = MRSH_BINOP_OR;
	}

	if (binop_type == -1) {
		return &pl->node;
	}

	linebreak(state);
	struct mrsh_node *node = and_or(state);
	assert(node != NULL);

	struct mrsh_binop *binop = mrsh_binop_create(binop_type, &pl->node, node);
	return &binop->node;
}

static struct mrsh_command_list *list(struct parser_state *state) {
	struct mrsh_node *node = and_or(state);
	if (node == NULL) {
		return NULL;
	}

	struct mrsh_command_list *cmd = calloc(1, sizeof(struct mrsh_command_list));
	cmd->node = node;

	int sep = separator_op(state);
	if (sep == '&') {
		cmd->ampersand = true;
	}

	return cmd;
}

static void complete_command(struct parser_state *state,
		struct mrsh_array *cmds) {
	struct mrsh_command_list *l = list(state);
	assert(l != NULL);
	mrsh_array_add(cmds, l);

	while (true) {
		l = list(state);
		if (l == NULL) {
			break;
		}
		mrsh_array_add(cmds, l);
	}
}

static void program(struct parser_state *state, struct mrsh_program *prog) {
	linebreak(state);
	if (accept(state, EOF_TOKEN)) {
		return;
	}

	complete_command(state, &prog->commands);

	while (newline_list(state) && state->sym.name != EOF_TOKEN) {
		complete_command(state, &prog->commands);
	}

	linebreak(state);
	expect(state, EOF_TOKEN);
}

struct mrsh_program *mrsh_parse(FILE *f) {
	struct parser_state state = {0};
	parser_init(&state, f);
	next_sym(&state);

	struct mrsh_program *prog = calloc(1, sizeof(struct mrsh_program));

	program(&state, prog);

	return prog;
}
