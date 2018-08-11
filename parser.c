#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "buffer.h"
#include "parser.h"

struct mrsh_parser *mrsh_parser_create(FILE *f) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));
	state->f = f;
	state->lineno = 1;
	return state;
}

struct mrsh_parser *mrsh_parser_create_from_buffer(const char *buf, size_t len) {
	struct mrsh_parser *state = calloc(1, sizeof(struct mrsh_parser));

	buffer_append(&state->buf, buf, len);
	buffer_append_char(&state->buf, '\0');

	state->lineno = 1;

	return state;
}

void mrsh_parser_destroy(struct mrsh_parser *state) {
	if (state == NULL) {
		return;
	}
	buffer_finish(&state->buf);
	free(state);
}

static size_t parser_peek(struct mrsh_parser *state, char *buf, size_t size) {
	if (state->f != NULL && size > state->buf.len) {
		size_t n_more = size - state->buf.len;
		char *dst = buffer_reserve(&state->buf, n_more);
		size_t n_read = fread(dst, 1, n_more, state->f);
		state->buf.len += n_read;
		if (n_read < n_more) {
			if (feof(state->f)) {
				buffer_append_char(&state->buf, '\0');
				size = state->buf.len;
			} else {
				// TODO: better error handling
				return 0;
			}
		}
	}
	if (state->f == NULL && size > state->buf.len) {
		size = state->buf.len;
	}

	if (buf != NULL) {
		memcpy(buf, state->buf.data, size);
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
		for (size_t i = 0; i < n; ++i) {
			if (state->buf.data[i] == '\n') {
				++state->lineno;
			}
		}
		memmove(state->buf.data, state->buf.data + n, state->buf.len - n);
		state->buf.len -= n;
	}
	return n;
}

static char parser_read_char(struct mrsh_parser *state) {
	char c = '\0';
	parser_read(state, &c, sizeof(char));
	return c;
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

static void parser_set_error(struct mrsh_parser *state, const char *msg) {
	state->here_documents.len = 0;

	fprintf(stderr, "mrsh:%d: syntax error: %s\n", state->lineno, msg);
	exit(EXIT_FAILURE);
}

// See section 2.3 Token Recognition
static void next_symbol(struct mrsh_parser *state) {
	state->has_sym = true;

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

static enum symbol_name get_symbol(struct mrsh_parser *state) {
	if (!state->has_sym) {
		next_symbol(state);
	}
	return state->sym;
}

static void consume_symbol(struct mrsh_parser *state) {
	state->has_sym = false;
}

static struct mrsh_token *single_quotes(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '\'');

	struct buffer buf = {0};

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

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	buffer_append_char(&buf, '\0');
	char *data = buffer_steal(&buf);
	struct mrsh_token_string *ts = mrsh_token_string_create(data, true);
	return &ts->token;
}

static size_t peek_name(struct mrsh_parser *state) {
	// In the shell command language, a word consisting solely of underscores,
	// digits, and alphabetics from the portable character set. The first
	// character of a name is not a digit.

	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];
		if (c != '_' && !isalnum(c)) {
			break;
		} else if (i == 0 && isdigit(c)) {
			break;
		}

		++i;
	}

	return i;
}

static size_t peek_token(struct mrsh_parser *state, char end) {
	size_t i = 0;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];

		switch (c) {
		case '\0':
		case '\n':
			return i;
		case '$':
		case '`':
		case '\'':
		case '"':
		case '\\': // TODO: allow backslash in tokens
			return 0;
		}

		if (is_operator_start(c) || isblank(c) || c == end) {
			return i;
		}

		++i;
	}
}

static struct mrsh_token *word(struct mrsh_parser *state, char end);

static struct mrsh_token *token_list(struct mrsh_parser *state, char end) {
	struct mrsh_array children = {0};

	while (true) {
		if (parser_peek_char(state) == end) {
			break;
		}

		struct mrsh_token *child = word(state, end);
		if (child == NULL) {
			break;
		}
		mrsh_array_add(&children, child);

		struct buffer buf = {0};
		while (true) {
			char c = parser_peek_char(state);
			if (!isblank(c)) {
				break;
			}
			buffer_append_char(&buf, parser_read_char(state));
		}
		if (buf.len == 0) {
			break; // word() ended on a non-blank char, stop here
		}
		buffer_append_char(&buf, '\0');
		struct mrsh_token_string *ts =
			mrsh_token_string_create(buffer_steal(&buf), false);
		mrsh_array_add(&children, &ts->token);
		buffer_finish(&buf);
	}

	if (children.len == 0) {
		return NULL;
	} else if (children.len == 1) {
		struct mrsh_token *child = children.data[0];
		mrsh_array_finish(&children);
		return child;
	} else {
		struct mrsh_token_list *tl = mrsh_token_list_create(&children, false);
		return &tl->token;
	}
}

static struct mrsh_token *expect_parameter_expression(
		struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '{');

	// TODO: ${#parameter}

	size_t name_len = peek_name(state);
	if (name_len == 0) {
		parser_set_error(state, "expected a parameter");
		return NULL;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';

	char *op = NULL;
	struct mrsh_token *arg = NULL;
	if (parser_peek_char(state) != '}') {
		char next[2];
		parser_peek(state, next, sizeof(next));
		bool two_letter_op = false;
		switch (next[0]) {
		case ':':
			if (strchr("-=?+", next[1]) == NULL) {
				parser_set_error(state, "expected a parameter operation");
				return NULL;
			}
			two_letter_op = true;
			break;
		case '-':
		case '=':
		case '?':
		case '+':
			break;
		case '%':
		case '#':
			two_letter_op = next[1] == next[0];
			break;
		default:
			parser_set_error(state, "expected a parameter operation");
			return NULL;
		}

		size_t op_len = two_letter_op ? 2 : 1;
		op = malloc(op_len + 1);
		parser_read(state, op, op_len);
		op[op_len] = '\0';

		arg = token_list(state, '}');
	}

	if (parser_read_char(state) != '}') {
		parser_set_error(state, "expected end of parameter");
		return NULL;
	}

	struct mrsh_token_parameter *tp =
		mrsh_token_parameter_create(name, op, arg);
	return &tp->token;
}

static struct mrsh_token *expect_parameter(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '$');

	if (parser_peek_char(state) == '{') {
		return expect_parameter_expression(state);
	}

	// TODO: ${expression}

	size_t name_len = peek_name(state);
	if (name_len == 0) {
		name_len = 1;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';

	struct mrsh_token_parameter *tp =
		mrsh_token_parameter_create(name, NULL, NULL);
	return &tp->token;
}

static struct mrsh_token *back_quotes(struct mrsh_parser *state) {
	// TODO: support nested back-quotes

	char c = parser_read_char(state);
	assert(c == '`');

	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			fprintf(stderr, "back quotes not terminated\n");
			exit(EXIT_FAILURE);
		}
		if (c == '`') {
			parser_read_char(state);
			break;
		}
		if (c == '\\') {
			// Quoted backslash
			char next[2];
			parser_peek(state, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '\\':
				parser_read_char(state);
				c = next[1];
				break;
			}
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	buffer_append_char(&buf, '\0');
	char *data = buffer_steal(&buf);
	struct mrsh_token_command *tc = mrsh_token_command_create(data, true);
	return &tc->token;
}

/**
 * Append a new string token to `children` with the contents of `buf`, and reset
 * `buf`.
 */
static void push_buffer_token_string(struct mrsh_array *children,
		struct buffer *buf) {
	if (buf->len == 0) {
		return;
	}

	buffer_append_char(buf, '\0');

	char *data = buffer_steal(buf);
	struct mrsh_token_string *ts = mrsh_token_string_create(data, false);
	mrsh_array_add(children, &ts->token);
}

static struct mrsh_token *double_quotes(struct mrsh_parser *state) {
	char c = parser_read_char(state);
	assert(c == '"');

	struct mrsh_array children = {0};
	struct buffer buf = {0};

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

		if (c == '$') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = expect_parameter(state);
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = back_quotes(state);
			mrsh_array_add(&children, t);
			continue;
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

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	push_buffer_token_string(&children, &buf);
	buffer_finish(&buf);

	struct mrsh_token_list *tl = mrsh_token_list_create(&children, true);
	return &tl->token;
}

static bool symbol(struct mrsh_parser *state, enum symbol_name sym) {
	return get_symbol(state) == sym;
}

static struct mrsh_token *word(struct mrsh_parser *state, char end) {
	if (!symbol(state, TOKEN)) {
		return NULL;
	}

	if (is_operator_start(parser_peek_char(state))
			|| parser_peek_char(state) == end) {
		return NULL;
	}

	struct mrsh_array children = {0};
	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0' || c == '\n' || c == end) {
			break;
		}

		if (c == '$') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = expect_parameter(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = back_quotes(state);
			mrsh_array_add(&children, t);
			continue;
		}

		// Quoting
		if (c == '\'') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = single_quotes(state);
			mrsh_array_add(&children, t);
			continue;
		}
		if (c == '"') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = double_quotes(state);
			mrsh_array_add(&children, t);
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
		} else if (is_operator_start(c) || isblank(c)) {
			break;
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	push_buffer_token_string(&children, &buf);
	buffer_finish(&buf);

	consume_symbol(state);

	if (children.len == 1) {
		struct mrsh_token *token = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return token;
	} else {
		struct mrsh_token_list *tl = mrsh_token_list_create(&children, false);
		return &tl->token;
	}
}

struct mrsh_token *mrsh_parse_token(struct mrsh_parser *state) {
	return token_list(state, 0);
}

static bool eof(struct mrsh_parser *state) {
	return symbol(state, EOF_TOKEN);
}

static bool newline(struct mrsh_parser *state) {
	if (!symbol(state, NEWLINE)) {
		return false;
	}
	char c = parser_read_char(state);
	assert(c == '\n');
	consume_symbol(state);
	return true;
}

static const char *operator_str(enum symbol_name sym) {
	for (size_t i = 0; i < sizeof(operators)/sizeof(operators[0]); ++i) {
		if (operators[i].name == sym) {
			return operators[i].str;
		}
	}
	return NULL;
}

static bool operator(struct mrsh_parser *state, enum symbol_name sym) {
	if (!symbol(state, sym)) {
		return false;
	}

	const char *str = operator_str(sym);
	assert(str != NULL);

	char buf[OPERATOR_MAX_LEN];
	parser_read(state, buf, strlen(str));
	assert(strncmp(str, buf, strlen(str)) == 0);
	consume_symbol(state);
	return true;
}

static bool token(struct mrsh_parser *state, const char *str) {
	if (!symbol(state, TOKEN)) {
		return false;
	}

	size_t len = strlen(str);
	assert(len > 0);

	if (len == 1 && !isalpha(str[0])) {
		if (parser_peek_char(state) != str[0]) {
			return false;
		}
		parser_read_char(state);
		consume_symbol(state);
		return true;
	}

	size_t token_len = peek_token(state, 0);
	if (len != token_len || strncmp(state->buf.data, str, token_len) != 0) {
		return false;
	}
	// assert(isalpha(str[i]));

	parser_read(state, NULL, len);
	consume_symbol(state);
	return true;
}

static bool expect_token(struct mrsh_parser *state, const char *str) {
	if (token(state, str)) {
		return true;
	}
	char msg[128];
	snprintf(msg, sizeof(msg), "unexpected token: expected %s\n", str);
	parser_set_error(state, msg);
	return false;
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

static bool io_here(struct mrsh_parser *state, struct mrsh_io_redirect *redir) {
	enum symbol_name sym = get_symbol(state);
	if (!operator(state, DLESS) && !operator(state, DLESSDASH)) {
		return false;
	}
	redir->op = strdup(operator_str(sym));

	redir->name = word(state, 0);
	if (redir->name == NULL) {
		parser_set_error(state,
			"expected a name after IO here-document redirection operator");
		return false;
	}
	// TODO: check redir->name only contains token strings and lists

	return true;
}

static struct mrsh_token *filename(struct mrsh_parser *state) {
	// TODO: Apply rule 2
	return word(state, 0);
}

static bool io_file(struct mrsh_parser *state,
		struct mrsh_io_redirect *redir) {
	enum symbol_name sym = get_symbol(state);
	if (token(state, "<")) {
		redir->op = strdup("<");
	} else if (token(state, ">")) {
		redir->op = strdup(">");
	} else if (operator(state, LESSAND)
			|| operator(state, GREATAND)
			|| operator(state, DGREAT)
			|| operator(state, CLOBBER)
			|| operator(state, LESSGREAT)) {
		redir->op = strdup(operator_str(sym));
	} else {
		return false;
	}

	redir->name = filename(state);
	if (redir->name == NULL) {
		parser_set_error(state,
			"expected a filename after IO file redirection operator");
		return false;
	}

	return true;
}

static int io_number(struct mrsh_parser *state) {
	char c = parser_peek_char(state);
	if (!isdigit(c)) {
		return -1;
	}

	char buf[2];
	parser_peek(state, buf, sizeof(buf));
	if (buf[1] != '<' && buf[1] != '>') {
		return -1;
	}

	parser_read_char(state);
	consume_symbol(state);
	return strtol(buf, NULL, 10);
}

static struct mrsh_io_redirect *io_redirect(struct mrsh_parser *state) {
	struct mrsh_io_redirect redir = {0};

	redir.io_number = io_number(state);

	if (io_file(state, &redir)) {
		struct mrsh_io_redirect *redir_ptr =
			calloc(1, sizeof(struct mrsh_io_redirect));
		memcpy(redir_ptr, &redir, sizeof(struct mrsh_io_redirect));
		return redir_ptr;
	}
	if (io_here(state, &redir)) {
		struct mrsh_io_redirect *redir_ptr =
			calloc(1, sizeof(struct mrsh_io_redirect));
		memcpy(redir_ptr, &redir, sizeof(struct mrsh_io_redirect));
		mrsh_array_add(&state->here_documents, redir_ptr);
		return redir_ptr;
	}

	if (redir.io_number >= 0) {
		parser_set_error(state, "expected an IO redirect after IO number");
	}
	return NULL;
}

static struct mrsh_assignment *assignment_word(struct mrsh_parser *state) {
	if (!symbol(state, TOKEN)) {
		return NULL;
	}

	size_t name_len = peek_name(state);
	if (name_len == 0) {
		return NULL;
	}

	parser_peek(state, NULL, name_len + 1);
	if (state->buf.data[name_len] != '=') {
		return NULL;
	}

	char *name = strndup(state->buf.data, name_len);
	parser_read(state, NULL, name_len + 1);
	struct mrsh_token *value = word(state, 0);
	consume_symbol(state);

	struct mrsh_assignment *assign = calloc(1, sizeof(struct mrsh_assignment));
	assign->name = name;
	assign->value = value;
	return assign;
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

static struct mrsh_token *cmd_name(struct mrsh_parser *state) {
	size_t token_len = peek_token(state, 0);
	if (token_len == 0) {
		return word(state, 0);
	}

	// TODO: optimize this
	for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i) {
		if (strlen(keywords[i]) == token_len &&
				strncmp(state->buf.data, keywords[i], token_len) == 0) {
			return NULL;
		}
	}

	// TODO: alias substitution

	char *str = malloc(token_len + 1);
	parser_read(state, str, token_len);
	str[token_len] = '\0';

	struct mrsh_token_string *ts = mrsh_token_string_create(str, false);
	return &ts->token;
}

static bool cmd_suffix(struct mrsh_parser *state,
		struct mrsh_simple_command *cmd) {
	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	struct mrsh_token *arg = word(state, 0);
	if (arg != NULL) {
		mrsh_array_add(&cmd->arguments, arg);
		return true;
	}

	return false;
}

static struct mrsh_simple_command *simple_command(struct mrsh_parser *state) {
	struct mrsh_simple_command cmd = {0};

	bool has_prefix = false;
	while (cmd_prefix(state, &cmd)) {
		has_prefix = true;
	}

	// TODO: alias substitution
	cmd.name = cmd_name(state);
	if (cmd.name == NULL && !has_prefix) {
		return NULL;
	} else if (cmd.name != NULL) {
		while (cmd_suffix(state, &cmd)) {
			// This space is intentionally left blank
		}
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

static bool expect_compound_list(struct mrsh_parser *state,
		struct mrsh_array *cmds) {
	linebreak(state);

	struct mrsh_command_list *l = term(state);
	if (l == NULL) {
		parser_set_error(state, "expected a term");
		return false;
	}
	mrsh_array_add(cmds, l);

	while (true) {
		l = term(state);
		if (l == NULL) {
			break;
		}
		mrsh_array_add(cmds, l);
	}

	return true;
}

static struct mrsh_brace_group *brace_group(struct mrsh_parser *state) {
	if (!token(state, "{")) {
		return NULL;
	}

	struct mrsh_array body = {0};
	if (!expect_compound_list(state, &body)) {
		return NULL;
	}

	if (!expect_token(state, "}")) {
		command_list_array_finish(&body);
		return NULL;
	}

	return mrsh_brace_group_create(&body);
}

static struct mrsh_command *else_part(struct mrsh_parser *state) {
	if (token(state, "elif")) {
		struct mrsh_array cond = {0};
		if (!expect_compound_list(state, &cond)) {
			return NULL;
		}

		if (!expect_token(state, "then")) {
			command_list_array_finish(&cond);
			return NULL;
		}

		struct mrsh_array body = {0};
		if (!expect_compound_list(state, &body)) {
			command_list_array_finish(&cond);
			return NULL;
		}

		struct mrsh_command *ep = else_part(state);

		struct mrsh_if_clause *ic = mrsh_if_clause_create(&cond, &body, ep);
		return &ic->command;
	}

	if (token(state, "else")) {
		struct mrsh_array body = {0};
		if (!expect_compound_list(state, &body)) {
			return NULL;
		}

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
	if (!expect_compound_list(state, &cond)) {
		goto error_cond;
	}

	if (!expect_token(state, "then")) {
		goto error_cond;
	}

	struct mrsh_array body = {0};
	if (!expect_compound_list(state, &body)) {
		goto error_body;
	}

	struct mrsh_command *ep = else_part(state);

	if (!expect_token(state, "fi")) {
		goto error_else_part;
	}

	return mrsh_if_clause_create(&cond, &body, ep);

error_else_part:
	mrsh_command_destroy(ep);
error_body:
	command_list_array_finish(&body);
error_cond:
	command_list_array_finish(&cond);
	return NULL;
}

static struct mrsh_command *compound_command(struct mrsh_parser *state);

static struct mrsh_function_definition *function_definition(
		struct mrsh_parser *state) {
	size_t name_len = peek_name(state);
	if (name_len == 0) {
		return NULL;
	}

	size_t i = name_len;
	while (true) {
		parser_peek(state, NULL, i + 1);

		char c = state->buf.data[i];
		if (c == '(') {
			break;
		} else if (!isblank(c)) {
			return NULL;
		}

		++i;
	}

	char *name = malloc(name_len + 1);
	parser_read(state, name, name_len);
	name[name_len] = '\0';
	consume_symbol(state);

	if (!expect_token(state, "(") || !expect_token(state, ")")) {
		return NULL;
	}

	linebreak(state);

	struct mrsh_command *cmd = compound_command(state);
	if (cmd == NULL) {
		parser_set_error(state, "expected a compount command");
		return NULL;
	}

	// TODO: compound_command redirect_list

	return mrsh_function_definition_create(name, cmd);
}

static struct mrsh_command *compound_command(struct mrsh_parser *state) {
	struct mrsh_brace_group *bg = brace_group(state);
	if (bg != NULL) {
		return &bg->command;
	}

	struct mrsh_if_clause *ic = if_clause(state);
	if (ic != NULL) {
		return &ic->command;
	}

	// TODO: subshell for_clause case_clause while_clause until_clause

	struct mrsh_function_definition *fd = function_definition(state);
	if (fd != NULL) {
		return &fd->command;
	}

	return NULL;
}

static struct mrsh_command *command(struct mrsh_parser *state) {
	struct mrsh_command *cmd = compound_command(state);
	if (cmd != NULL) {
		return cmd;
	}

	// TODO: compound_command redirect_list

	struct mrsh_simple_command *sc = simple_command(state);
	if (sc != NULL) {
		return &sc->command;
	}

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
		if (cmd == NULL) {
			// TODO: free commands
			parser_set_error(state, "expected a command");
			return NULL;
		}
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
	if (node == NULL) {
		mrsh_node_destroy(&pl->node);
		parser_set_error(state, "expected an AND-OR list");
		return NULL;
	}

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

static struct mrsh_token *here_document_line(struct mrsh_parser *state) {
	struct mrsh_array children = {0};
	struct buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			break;
		}

		if (c == '$') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = expect_parameter(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_token_string(&children, &buf);
			struct mrsh_token *t = back_quotes(state);
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '\\') {
			// Here-document backslash, same semantics as quoted backslash
			// except double-quotes are not special
			char next[2];
			parser_peek(state, next, sizeof(next));
			switch (next[1]) {
			case '$':
			case '`':
			case '\\':
				parser_read_char(state);
				c = next[1];
				break;
			}
		}

		parser_read_char(state);
		buffer_append_char(&buf, c);
	}

	push_buffer_token_string(&children, &buf);
	buffer_finish(&buf);

	if (children.len == 1) {
		struct mrsh_token *token = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return token;
	} else {
		struct mrsh_token_list *tl = mrsh_token_list_create(&children, false);
		return &tl->token;
	}
}

static bool is_token_quoted(struct mrsh_token *token) {
	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);
		return ts->single_quoted;
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		if (tl->double_quoted) {
			return true;
		}
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			if (is_token_quoted(child)) {
				return true;
			}
		}
		return false;
	default:
		assert(false);
	}
}

static bool expect_here_document(struct mrsh_parser *state,
		struct mrsh_io_redirect *redir, const char *delim) {
	bool trim_tabs = strcmp(redir->op, "<<-") == 0;
	bool expand_lines = !is_token_quoted(redir->name);

	struct buffer buf = {0};
	while (true) {
		buf.len = 0;
		while (true) {
			char c = parser_peek_char(state);
			if (c == '\0' || c == '\n') {
				break;
			}

			buffer_append_char(&buf, parser_read_char(state));
		}
		buffer_append_char(&buf, '\0');

		const char *line = buf.data;
		if (trim_tabs) {
			while (line[0] == '\t') {
				++line;
			}
		}

		if (strcmp(line, delim) == 0) {
			break;
		}
		if (eof(state)) {
			parser_set_error(state, "unterminated here-document");
			return false;
		}
		bool ok = newline(state);
		assert(ok);

		struct mrsh_token *token;
		if (expand_lines) {
			struct mrsh_parser *subparser =
				mrsh_parser_create_from_buffer(line, strlen(line));
			token = here_document_line(subparser);
			mrsh_parser_destroy(subparser);
		} else {
			struct mrsh_token_string *ts =
				mrsh_token_string_create(strdup(line), true);
			token = &ts->token;
		}

		mrsh_array_add(&redir->here_document, token);
	}
	buffer_finish(&buf);

	return true;
}

static bool expect_complete_command(struct mrsh_parser *state,
		struct mrsh_array *cmds) {
	struct mrsh_command_list *l = list(state);
	if (l == NULL) {
		parser_set_error(state, "expected a complete command");
		return false;
	}
	mrsh_array_add(cmds, l);

	while (true) {
		l = list(state);
		if (l == NULL) {
			break;
		}
		mrsh_array_add(cmds, l);
	}

	if (state->here_documents.len > 0) {
		for (size_t i = 0; i < state->here_documents.len; ++i) {
			struct mrsh_io_redirect *redir = state->here_documents.data[i];

			if (!newline(state)) {
				parser_set_error(state,
					"expected a newline followed by a here-document");
				return false;
			}

			char *delim = mrsh_token_str(redir->name);
			bool ok = expect_here_document(state, redir, delim);
			free(delim);
			if (!ok) {
				return false;
			}
		}

		state->here_documents.len = 0;
	}

	return true;
}

static struct mrsh_program *program(struct mrsh_parser *state) {
	struct mrsh_program *prog = calloc(1, sizeof(struct mrsh_program));
	if (prog == NULL) {
		return NULL;
	}

	linebreak(state);
	if (eof(state)) {
		return prog;
	}

	if (!expect_complete_command(state, &prog->body)) {
		mrsh_program_destroy(prog);
		return NULL;
	}

	while (newline_list(state)) {
		if (eof(state)) {
			return prog;
		}

		if (!expect_complete_command(state, &prog->body)) {
			mrsh_program_destroy(prog);
			return NULL;
		}
	}

	linebreak(state);
	return prog;
}

struct mrsh_program *mrsh_parse_line(struct mrsh_parser *state) {
	if (eof(state) || newline(state)) {
		return NULL;
	}

	struct mrsh_program *prog = calloc(1, sizeof(struct mrsh_program));
	if (prog == NULL) {
		return NULL;
	}

	if (!expect_complete_command(state, &prog->body)) {
		mrsh_program_destroy(prog);
		return NULL;
	}
	if (!eof(state) && !newline(state)) {
		mrsh_program_destroy(prog);
		parser_set_error(state, "expected a newline");
		return NULL;
	}

	return prog;
}

struct mrsh_program *mrsh_parse(FILE *f) {
	struct mrsh_parser *state = mrsh_parser_create(f);
	struct mrsh_program *prog = program(state);
	mrsh_parser_destroy(state);
	return prog;
}

bool mrsh_parser_eof(struct mrsh_parser *state) {
	return state->has_sym && state->sym == EOF_TOKEN;
}
