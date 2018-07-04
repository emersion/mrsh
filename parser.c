#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void next_symbol(struct mrsh_parser *state);

struct mrsh_parser *mrsh_parser_create(FILE *f) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));

	state->f = f;

	state->peek_cap = 128;
	state->peek = malloc(state->peek_cap);
	state->peek_len = 0;

	next_symbol(state);

	return state;
}

void mrsh_parser_destroy(struct mrsh_parser *state) {
	if (state == NULL) {
		return;
	}
	free(state->peek);
	free(state);
}

static size_t parser_peek(struct mrsh_parser *state, char *buf, size_t size) {
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

static char parser_peek_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_peek(state, &c, sizeof(char));
	return c;
}

static size_t parser_read(struct mrsh_parser *state, char *buf, size_t size) {
	size_t n = parser_peek(state, buf, size);
	if (n > 0) {
		memmove(state->peek, state->peek + n, state->peek_len - n);
		state->peek_len -= n;
	}
	return n;
}

static char parser_read_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_read(state, &c, sizeof(char));
	return c;
}

static bool accept_str(struct mrsh_parser *state, const char *str) {
	size_t len = strlen(str);
	assert(len > 0);

	// TODO: optimize this
	char next[len];
	for (size_t n = 1; n <= len; ++n) {
		size_t n_read = parser_peek(state, next, n);
		if (n_read < n || strncmp(next, str, n) != 0) {
			return false;
		}
	}

	parser_read(state, next, len);
	return true;
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

/*static void single_quotes(struct mrsh_parser *state) {
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

static void double_quotes(struct mrsh_parser *state) {
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

static void word(struct mrsh_parser *state) {
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
}*/

static char *word(struct mrsh_parser *state) {
	if (state->sym != TOKEN) {
		return NULL;
	}

	char *str = malloc(128); // TODO
	char *cur = str;

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
			// TODO
			//single_quotes(state);
			//continue;
			fprintf(stderr, "not yet implemented\n");
			exit(EXIT_FAILURE);
		}
		if (c == '"') {
			// TODO
			//double_quotes(state);
			//continue;
			fprintf(stderr, "not yet implemented\n");
			exit(EXIT_FAILURE);
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

		if (is_operator_start(c)) {
			if (first) {
				cur[0] = c;
				cur++;
				parser_read_char(state);
			}
			break;
		}
		if (isblank(c)) {
			break;
		}

		cur[0] = c;
		cur++;
		parser_read_char(state);
		first = false;
	}
	cur[0] = '\0';

	next_symbol(state);
	return str;
}

// See section 2.3 Token Recognition
static void next_symbol(struct mrsh_parser *state) {
	char c = parser_peek_char(state);

	if (c == '\0') {
		state->sym = EOF_TOKEN;
		return;
	}
	if (c == '\n') {
		state->sym = NEWLINE;
		return;
	}

	if (is_operator_start(c)) {
		char next[OPERATOR_MAX_LEN];
		for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
			const char *str = operators[i].str;
			size_t n = strlen(str);
			size_t n_read = parser_peek(state, next, n);
			if (n_read == n && strncmp(next, str, n) == 0) {
				state->sym = operators[i].name;
				return;
			}
		}
	}

	if (isblank(c)) {
		parser_read_char(state);
		next_symbol(state);
		return;
	}

	if (c == '#') {
		while (true) {
			char c = parser_peek_char(state);
			if (c == '\0' || c == '\n') {
				break;
			}
			parser_read_char(state);
		}
		next_symbol(state);
		return;
	}

	state->sym = TOKEN;
}

static bool eof(struct mrsh_parser *state) {
	return state->sym == EOF_TOKEN;
}

static bool newline(struct mrsh_parser *state) {
	if (state->sym != NEWLINE) {
		return false;
	}
	char c = parser_read_char(state);
	assert(c == '\n');
	next_symbol(state);
	return true;
}

static bool operator(struct mrsh_parser *state, enum symbol_name sym) {
	if (state->sym != sym) {
		return false;
	}

	const char *str = NULL;
	for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
		if (operators[i].name == sym) {
			str = operators[i].str;
			break;
		}
	}
	assert(str != NULL);

	char buf[OPERATOR_MAX_LEN];
	parser_read(state, buf, strlen(str));
	assert(strncmp(str, buf, strlen(str)) == 0);
	next_symbol(state);
	return true;
}

static bool token(struct mrsh_parser *state, const char *str) {
	if (state->sym != TOKEN) {
		return false;
	}
	if (!accept_str(state, str)) {
		return false;
	}
	next_symbol(state);
	return true;
}

static void expect_token(struct mrsh_parser *state, const char *str) {
	if (token(state, str)) {
		return;
	}
	fprintf(stderr, "unexpected token: expected %s\n", str);
	exit(EXIT_FAILURE);
}

static void linebreak(struct mrsh_parser *state) {
	while (newline(state)) {
		// This space is intentionally left blank
	}
}

static bool newline_list(struct mrsh_parser *state) {
	if (!newline(state)) {
		return false;
	}

	linebreak(state);
	return true;
}

static int separator_op(struct mrsh_parser *state) {
	if (token(state, "&")) {
		return '&';
	}
	if (token(state, ";")) {
		return ';';
	}
	return -1;
}

/*static bool io_here(struct mrsh_parser *state) {
	return false; // TODO
}

static char *filename(struct mrsh_parser *state) {
	// TODO: Apply rule 2
	return take_token(state);
}

static bool io_file(struct mrsh_parser *state,
		struct mrsh_io_redirect *redir) {
	char *str = strdup(state->sym.str);
	enum symbol_name name = state->sym.name;
	if (token(state, "<") || token(state, ">")) {
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
}*/

static struct mrsh_io_redirect *io_redirect(struct mrsh_parser *state) {
	/*struct mrsh_io_redirect redir = {
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

	return NULL;*/
	return NULL;
}

static struct mrsh_assignment *assignment_word(struct mrsh_parser *state) {
	/*if (state->sym.name != TOKEN && state->sym.name != WORD) {
		return NULL;
	}

	// TODO: check that the equal sign is unquoted
	const char *pos = strchr(state->sym.str, '=');
	if (pos == NULL || pos == state->sym.str) {
		return NULL;
	}
	// TODO: check that chars before = form a valid name

	struct mrsh_assignment *assign = calloc(1, sizeof(struct mrsh_assignment));
	assign->name = strndup(state->sym.str, pos - state->sym.str);
	assign->value = strdup(pos + 1);
	next_sym(state);
	return assign;*/
	return NULL;
}

static bool cmd_prefix(struct mrsh_parser *state,
		struct mrsh_simple_command *cmd) {
	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	struct mrsh_assignment *assign = assignment_word(state);
	if (assign != NULL) {
		mrsh_array_add(&cmd->assignments, assign);
		return true;
	}

	return false;
}

static bool cmd_suffix(struct mrsh_parser *state,
		struct mrsh_simple_command *cmd) {
	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	char *arg = word(state);
	if (arg != NULL) {
		mrsh_array_add(&cmd->arguments, arg);
		return true;
	}

	return false;
}

static struct mrsh_simple_command *simple_command(struct mrsh_parser *state) {
	struct mrsh_simple_command cmd = {0};

	while (cmd_prefix(state, &cmd)) {
		// This space is intentionally left blank
	}

	cmd.name = word(state);
	if (cmd.name == NULL) {
		return NULL;
	}

	while (cmd_suffix(state, &cmd)) {
		// This space is intentionally left blank
	}

	return mrsh_simple_command_create(cmd.name, &cmd.arguments,
		&cmd.io_redirects, &cmd.assignments);
}

static int separator(struct mrsh_parser *state) {
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

static struct mrsh_node *and_or(struct mrsh_parser *state);

static struct mrsh_command_list *term(struct mrsh_parser *state) {
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

static void compound_list(struct mrsh_parser *state, struct mrsh_array *cmds) {
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

static struct mrsh_brace_group *brace_group(struct mrsh_parser *state) {
	if (!token(state, "{")) {
		return NULL;
	}

	struct mrsh_array body = {0};
	compound_list(state, &body);

	expect_token(state, "}");
	return mrsh_brace_group_create(&body);
}

static struct mrsh_command *else_part(struct mrsh_parser *state) {
	if (token(state, "elif")) {
		struct mrsh_array cond = {0};
		compound_list(state, &cond);

		expect_token(state, "then");

		struct mrsh_array body = {0};
		compound_list(state, &body);

		struct mrsh_command *ep = else_part(state);

		struct mrsh_if_clause *ic = mrsh_if_clause_create(&cond, &body, ep);
		return &ic->command;
	}

	if (token(state, "else")) {
		struct mrsh_array body = {0};
		compound_list(state, &body);

		struct mrsh_brace_group *bg = mrsh_brace_group_create(&body);
		return &bg->command;
	}

	return NULL;
}

static struct mrsh_if_clause *if_clause(struct mrsh_parser *state) {
	if (!token(state, "if")) {
		return NULL;
	}

	struct mrsh_array cond = {0};
	compound_list(state, &cond);

	expect_token(state, "then");

	struct mrsh_array body = {0};
	compound_list(state, &body);

	struct mrsh_command *ep = else_part(state);

	expect_token(state, "fi");

	return mrsh_if_clause_create(&cond, &body, ep);
}

static struct mrsh_command *command(struct mrsh_parser *state) {
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

static struct mrsh_pipeline *pipeline(struct mrsh_parser *state) {
	bool bang = token(state, "!");

	struct mrsh_command *cmd = command(state);
	if (cmd == NULL) {
		return NULL;
	}

	struct mrsh_array commands = {0};
	mrsh_array_add(&commands, cmd);

	while (token(state, "|")) {
		linebreak(state);
		struct mrsh_command *cmd = command(state);
		mrsh_array_add(&commands, cmd);
	}

	return mrsh_pipeline_create(&commands, bang);
}

static struct mrsh_node *and_or(struct mrsh_parser *state) {
	struct mrsh_pipeline *pl = pipeline(state);
	if (pl == NULL) {
		return NULL;
	}

	int binop_type = -1;
	if (operator(state, AND_IF)) {
		binop_type = MRSH_BINOP_AND;
	} else if (operator(state, OR_IF)) {
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

static struct mrsh_command_list *list(struct mrsh_parser *state) {
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

static void complete_command(struct mrsh_parser *state,
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

static struct mrsh_program *program(struct mrsh_parser *state) {
	struct mrsh_program *prog = calloc(1, sizeof(struct mrsh_program));

	linebreak(state);
	if (eof(state)) {
		return prog;
	}

	complete_command(state, &prog->body);

	while (newline_list(state)) {
		if (eof(state)) {
			return prog;
		}

		complete_command(state, &prog->body);
	}

	linebreak(state);
	if (eof(state)) {
		assert(false);
	}
	return prog;
}

struct mrsh_command_list *mrsh_parse_command_list(struct mrsh_parser *state) {
	linebreak(state);
	if (eof(state)) {
		return NULL;
	}

	return list(state);
}

struct mrsh_program *mrsh_parse(FILE *f) {
	struct mrsh_parser *state = mrsh_parser_create(f);
	struct mrsh_program *prog = program(state);
	mrsh_parser_destroy(state);
	return prog;
}
