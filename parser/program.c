#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <mrsh/buffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"

static const char *operator_str(enum symbol_name sym) {
	for (size_t i = 0; i < operators_len; ++i) {
		if (operators[i].name == sym) {
			return operators[i].str;
		}
	}
	return NULL;
}

static bool operator(struct mrsh_parser *state, enum symbol_name sym,
		struct mrsh_range *range) {
	if (!symbol(state, sym)) {
		return false;
	}

	struct mrsh_position begin = state->pos;

	const char *str = operator_str(sym);
	assert(str != NULL);

	char buf[operators_max_str_len];
	parser_read(state, buf, strlen(str));
	assert(strncmp(str, buf, strlen(str)) == 0);

	if (range != NULL) {
		range->begin = begin;
		range->end = state->pos;
	}

	consume_symbol(state);
	return true;
}

static int separator_op(struct mrsh_parser *state) {
	if (token(state, "&", NULL)) {
		return '&';
	}
	if (token(state, ";", NULL)) {
		return ';';
	}
	return -1;
}

static size_t peek_alias(struct mrsh_parser *state) {
	size_t n = peek_word(state, 0);

	for (size_t i = 0; i < n; ++i) {
		char c = state->buf.data[i];
		switch (c) {
		case '_':
		case '!':
		case '%':
		case ',':
		case '@':
			break;
		default:
			if (!isalnum(c)) {
				return 0;
			}
		}
	}

	return n;
}

static void apply_aliases(struct mrsh_parser *state) {
	if (state->alias == NULL) {
		return;
	}

	const char *last_repl = NULL;
	while (true) {
		if (!symbol(state, TOKEN)) {
			return;
		}

		size_t alias_len = peek_alias(state);
		if (alias_len == 0) {
			return;
		}

		char *name = strndup(state->buf.data, alias_len);
		const char *repl = state->alias(name, state->alias_user_data);
		free(name);
		if (repl == NULL || last_repl == repl) {
			break;
		}

		size_t trailing_len = state->buf.len - alias_len;
		size_t repl_len = strlen(repl);
		if (repl_len > alias_len) {
			mrsh_buffer_reserve(&state->buf, repl_len - alias_len);
		}
		memmove(&state->buf.data[repl_len], &state->buf.data[alias_len],
			state->buf.len - alias_len);
		memcpy(state->buf.data, repl, repl_len);
		state->buf.len = repl_len + trailing_len;

		// TODO: fixup state->pos
		// TODO: if repl's last char is blank, replace next alias too

		consume_symbol(state);
		last_repl = repl;
	}
}

static bool io_here(struct mrsh_parser *state, struct mrsh_io_redirect *redir) {
	if (operator(state, DLESS, &redir->op_range)) {
		redir->op = MRSH_IO_DLESS;
	} else if (operator(state, DLESSDASH, &redir->op_range)) {
		redir->op = MRSH_IO_DLESSDASH;
	} else {
		return false;
	}

	redir->name = word(state, 0);
	if (redir->name == NULL) {
		parser_set_error(state,
			"expected a name after IO here-document redirection operator");
		return false;
	}
	// TODO: check redir->name only contains word strings and lists

	return true;
}

static struct mrsh_word *filename(struct mrsh_parser *state) {
	// TODO: Apply rule 2
	return word(state, 0);
}

static int io_redirect_op(struct mrsh_parser *state, struct mrsh_range *range) {
	if (token(state, "<", range)) {
		return MRSH_IO_LESS;
	} else if (token(state, ">", range)) {
		return MRSH_IO_GREAT;
	} else if (operator(state, LESSAND, range)) {
		return MRSH_IO_LESSAND;
	} else if (operator(state, GREATAND, range)) {
		return MRSH_IO_GREATAND;
	} else if (operator(state, DGREAT, range)) {
		return MRSH_IO_DGREAT;
	} else if (operator(state, CLOBBER, range)) {
		return MRSH_IO_CLOBBER;
	} else if (operator(state, LESSGREAT, range)) {
		return MRSH_IO_LESSGREAT;
	} else {
		return -1;
	}
}

static bool io_file(struct mrsh_parser *state,
		struct mrsh_io_redirect *redir) {
	int op = io_redirect_op(state, &redir->op_range);
	if (op < 0) {
		return false;
	}
	redir->op = op;

	redir->name = filename(state);
	if (redir->name == NULL) {
		parser_set_error(state,
			"expected a filename after IO file redirection operator");
		return false;
	}

	return true;
}

static int io_number(struct mrsh_parser *state) {
	if (!symbol(state, TOKEN)) {
		return -1;
	}

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

	struct mrsh_position io_number_pos = state->pos;
	redir.io_number = io_number(state);
	if (redir.io_number >= 0) {
		redir.io_number_pos = io_number_pos;
	}

	redir.op_range.begin = state->pos;
	if (io_file(state, &redir)) {
		struct mrsh_io_redirect *redir_ptr =
			calloc(1, sizeof(struct mrsh_io_redirect));
		memcpy(redir_ptr, &redir, sizeof(struct mrsh_io_redirect));
		redir.op_range.end = state->pos;
		return redir_ptr;
	}
	if (io_here(state, &redir)) {
		struct mrsh_io_redirect *redir_ptr =
			calloc(1, sizeof(struct mrsh_io_redirect));
		memcpy(redir_ptr, &redir, sizeof(struct mrsh_io_redirect));
		redir.op_range.end = state->pos;
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

	size_t name_len = peek_name(state, false);
	if (name_len == 0) {
		return NULL;
	}

	parser_peek(state, NULL, name_len + 1);
	if (state->buf.data[name_len] != '=') {
		return NULL;
	}

	struct mrsh_range name_range;
	char *name = read_token(state, name_len, &name_range);

	struct mrsh_position equal_pos = state->pos;
	parser_read(state, NULL, 1);

	struct mrsh_word *value = word(state, 0);
	if (value == NULL) {
		value = &mrsh_word_string_create(strdup(""), false)->word;
	}

	struct mrsh_assignment *assign = calloc(1, sizeof(struct mrsh_assignment));
	assign->name = name;
	assign->value = value;
	assign->name_range = name_range;
	assign->equal_pos = equal_pos;
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

static struct mrsh_word *cmd_name(struct mrsh_parser *state) {
	apply_aliases(state);

	size_t word_len = peek_word(state, 0);
	if (word_len == 0) {
		return word(state, 0);
	}

	// TODO: optimize this
	for (size_t i = 0; i < keywords_len; ++i) {
		if (strlen(keywords[i]) == word_len &&
				strncmp(state->buf.data, keywords[i], word_len) == 0) {
			return NULL;
		}
	}

	struct mrsh_range range;
	char *str = read_token(state, word_len, &range);

	struct mrsh_word_string *ws = mrsh_word_string_create(str, false);
	ws->range = range;
	return &ws->word;
}

static bool cmd_suffix(struct mrsh_parser *state,
		struct mrsh_simple_command *cmd) {
	struct mrsh_io_redirect *redir = io_redirect(state);
	if (redir != NULL) {
		mrsh_array_add(&cmd->io_redirects, redir);
		return true;
	}

	struct mrsh_word *arg = word(state, 0);
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

static struct mrsh_and_or_list *and_or(struct mrsh_parser *state);

static struct mrsh_command_list *term(struct mrsh_parser *state) {
	struct mrsh_and_or_list *and_or_list = and_or(state);
	if (and_or_list == NULL) {
		return NULL;
	}

	struct mrsh_command_list *cmd = mrsh_command_list_create();
	cmd->and_or_list = and_or_list;

	struct mrsh_position separator_pos = state->pos;
	int sep = separator(state);
	if (sep == '&') {
		cmd->ampersand = true;
	}
	if (sep >= 0) {
		cmd->separator_pos = separator_pos;
	}

	return cmd;
}

static bool compound_list(struct mrsh_parser *state, struct mrsh_array *cmds) {
	linebreak(state);

	struct mrsh_command_list *l = term(state);
	if (l == NULL) {
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

static bool expect_compound_list(struct mrsh_parser *state,
		struct mrsh_array *cmds) {
	if (!compound_list(state, cmds)) {
		parser_set_error(state, "expected a compound list");
		return false;
	}

	return true;
}

static struct mrsh_brace_group *brace_group(struct mrsh_parser *state) {
	struct mrsh_position lbrace_pos = state->pos;
	if (!token(state, "{", NULL)) {
		return NULL;
	}

	struct mrsh_array body = {0};
	if (!expect_compound_list(state, &body)) {
		return NULL;
	}

	struct mrsh_position rbrace_pos = state->pos;
	if (!expect_token(state, "}", NULL)) {
		command_list_array_finish(&body);
		return NULL;
	}

	struct mrsh_brace_group *bg = mrsh_brace_group_create(&body);
	bg->lbrace_pos = lbrace_pos;
	bg->rbrace_pos = rbrace_pos;
	return bg;
}

static struct mrsh_subshell *subshell(struct mrsh_parser *state) {
	struct mrsh_position lparen_pos = state->pos;
	if (!token(state, "(", NULL)) {
		return NULL;
	}

	struct mrsh_array body = {0};
	if (!expect_compound_list(state, &body)) {
		return NULL;
	}

	struct mrsh_position rparen_pos = state->pos;
	if (!expect_token(state, ")", NULL)) {
		command_list_array_finish(&body);
		return NULL;
	}

	struct mrsh_subshell *s = mrsh_subshell_create(&body);
	s->lparen_pos = lparen_pos;
	s->rparen_pos = rparen_pos;
	return s;
}

static struct mrsh_command *else_part(struct mrsh_parser *state) {
	struct mrsh_range if_range = {0};
	if (token(state, "elif", &if_range)) {
		struct mrsh_array cond = {0};
		if (!expect_compound_list(state, &cond)) {
			return NULL;
		}

		struct mrsh_range then_range;
		if (!expect_token(state, "then", &then_range)) {
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
		ic->if_range = if_range;
		ic->then_range = then_range;
		return &ic->command;
	}

	if (token(state, "else", NULL)) {
		struct mrsh_array body = {0};
		if (!expect_compound_list(state, &body)) {
			return NULL;
		}

		// TODO: position information is missing
		struct mrsh_brace_group *bg = mrsh_brace_group_create(&body);
		return &bg->command;
	}

	return NULL;
}

static struct mrsh_if_clause *if_clause(struct mrsh_parser *state) {
	struct mrsh_range if_range;
	if (!token(state, "if", &if_range)) {
		return NULL;
	}

	struct mrsh_array cond = {0};
	if (!expect_compound_list(state, &cond)) {
		goto error_cond;
	}

	struct mrsh_range then_range;
	if (!expect_token(state, "then", &then_range)) {
		goto error_cond;
	}

	struct mrsh_array body = {0};
	if (!expect_compound_list(state, &body)) {
		goto error_body;
	}

	struct mrsh_command *ep = else_part(state);

	struct mrsh_range fi_range;
	if (!expect_token(state, "fi", &fi_range)) {
		goto error_else_part;
	}

	struct mrsh_if_clause *ic = mrsh_if_clause_create(&cond, &body, ep);
	ic->if_range = if_range;
	ic->then_range = then_range;
	ic->fi_range = fi_range;
	return ic;

error_else_part:
	mrsh_command_destroy(ep);
error_body:
	command_list_array_finish(&body);
error_cond:
	command_list_array_finish(&cond);
	return NULL;
}

static bool sequential_sep(struct mrsh_parser *state) {
	if (token(state, ";", NULL)) {
		linebreak(state);
		return true;
	}
	return newline_list(state);
}

static void wordlist(struct mrsh_parser *state,
		struct mrsh_array *words) {
	while (true) {
		struct mrsh_word *w = word(state, 0);
		if (w == NULL) {
			break;
		}
		mrsh_array_add(words, w);
	}
}

static bool expect_do_group(struct mrsh_parser *state,
		struct mrsh_array *body, struct mrsh_range *do_range,
		struct mrsh_range *done_range) {
	if (!token(state, "do", do_range)) {
		parser_set_error(state, "expected 'do'");
		return false;
	}

	if (!expect_compound_list(state, body)) {
		return false;
	}

	if (!token(state, "done", done_range)) {
		parser_set_error(state, "expected 'done'");
		command_list_array_finish(body);
		return false;
	}

	return true;
}

static struct mrsh_for_clause *for_clause(struct mrsh_parser *state) {
	struct mrsh_range for_range;
	if (!token(state, "for", &for_range)) {
		return NULL;
	}

	size_t name_len = peek_name(state, false);
	if (name_len == 0) {
		parser_set_error(state, "expected name");
		return NULL;
	}

	struct mrsh_range name_range;
	char *name = read_token(state, name_len, &name_range);

	linebreak(state);

	struct mrsh_range in_range = {0};
	bool in = token(state, "in", &in_range);

	// TODO: save sequential_sep position, if any
	struct mrsh_array words = {0};
	if (in) {
		wordlist(state, &words);

		if (!sequential_sep(state)) {
			parser_set_error(state, "expected sequential separator");
			goto error_words;
		}
	} else {
		sequential_sep(state);
	}

	struct mrsh_array body = {0};
	struct mrsh_range do_range, done_range;
	if (!expect_do_group(state, &body, &do_range, &done_range)) {
		goto error_words;
	}

	struct mrsh_for_clause *fc =
		mrsh_for_clause_create(name, in, &words, &body);
	fc->for_range = for_range;
	fc->name_range = name_range;
	fc->in_range = in_range;
	fc->do_range = do_range;
	fc->done_range = done_range;
	return fc;

error_words:
	for (size_t i = 0; i < words.len; ++i) {
		struct mrsh_word *word = words.data[i];
		mrsh_word_destroy(word);
	}
	mrsh_array_finish(&words);
	free(name);
	return NULL;
}

static struct mrsh_loop_clause *loop_clause(struct mrsh_parser *state) {
	enum mrsh_loop_type type;
	struct mrsh_range while_until_range;
	if (token(state, "while", &while_until_range)) {
		type = MRSH_LOOP_WHILE;
	} else if (token(state, "until", &while_until_range)) {
		type = MRSH_LOOP_UNTIL;
	} else {
		return NULL;
	}

	struct mrsh_array condition = {0};
	if (!expect_compound_list(state, &condition)) {
		return NULL;
	}

	struct mrsh_array body = {0};
	struct mrsh_range do_range, done_range;
	if (!expect_do_group(state, &body, &do_range, &done_range)) {
		command_list_array_finish(&condition);
		return NULL;
	}

	struct mrsh_loop_clause *fc =
		mrsh_loop_clause_create(type, &condition, &body);
	fc->while_until_range = while_until_range;
	fc->do_range = do_range;
	fc->done_range = done_range;
	return fc;
}

static struct mrsh_case_item *expect_case_item(struct mrsh_parser *state,
		bool *dsemi) {
	struct mrsh_position lparen_pos = state->pos;
	if (!token(state, "(", NULL)) {
		lparen_pos = (struct mrsh_position){0};
	}

	struct mrsh_word *w = word(state, 0);
	if (w == NULL) {
		parser_set_error(state, "expected a word");
		return NULL;
	}

	struct mrsh_array patterns = {0};
	mrsh_array_add(&patterns, w);

	while (token(state, "|", NULL)) {
		struct mrsh_word *w = word(state, 0);
		if (w == NULL) {
			parser_set_error(state, "expected a word");
			return NULL;
		}
		mrsh_array_add(&patterns, w);
	}

	struct mrsh_position rparen_pos = state->pos;
	if (!expect_token(state, ")", NULL)) {
		goto error_patterns;
	}

	// It's okay if there's no body
	struct mrsh_array body = {0};
	compound_list(state, &body);
	if (mrsh_parser_error(state, NULL)) {
		goto error_patterns;
	}

	struct mrsh_range dsemi_range = {0};
	*dsemi = operator(state, DSEMI, &dsemi_range);
	if (*dsemi) {
		linebreak(state);
	}

	struct mrsh_case_item *item = calloc(1, sizeof(struct mrsh_case_item));
	if (item == NULL) {
		goto error_body;
	}
	item->patterns = patterns;
	item->body = body;
	item->lparen_pos = lparen_pos;
	item->rparen_pos = rparen_pos;
	item->dsemi_range = dsemi_range;
	return item;

error_body:
	command_list_array_finish(&body);
error_patterns:
	for (size_t i = 0; i < patterns.len; ++i) {
		struct mrsh_word *w = patterns.data[i];
		mrsh_word_destroy(w);
	}
	mrsh_array_finish(&patterns);
	return NULL;
}

static struct mrsh_case_clause *case_clause(struct mrsh_parser *state) {
	struct mrsh_range case_range;
	if (!token(state, "case", &case_range)) {
		return NULL;
	}

	struct mrsh_word *w = word(state, 0);
	if (w == NULL) {
		parser_set_error(state, "expected a word");
		return NULL;
	}

	linebreak(state);

	struct mrsh_range in_range;
	if (!expect_token(state, "in", &in_range)) {
		goto error_word;
	}

	linebreak(state);

	bool dsemi = false;
	struct mrsh_array items = {0};
	struct mrsh_range esac_range;
	while (!token(state, "esac", &esac_range)) {
		struct mrsh_case_item *item = expect_case_item(state, &dsemi);
		if (item == NULL) {
			goto error_items;
		}
		mrsh_array_add(&items, item);

		if (!dsemi) {
			// Only the last case can omit `;;`
			if (!expect_token(state, "esac", &esac_range)) {
				goto error_items;
			}
			break;
		}
	}

	struct mrsh_case_clause *cc = mrsh_case_clause_create(w, &items);
	cc->case_range = case_range;
	cc->in_range = in_range;
	cc->esac_range = esac_range;
	return cc;

error_items:
	for (size_t i = 0; i < items.len; ++i) {
		struct mrsh_case_item *item = items.data[i];
		case_item_destroy(item);
	}
	mrsh_array_finish(&items);
error_word:
	mrsh_word_destroy(w);
	return NULL;
}

static struct mrsh_command *compound_command(struct mrsh_parser *state);

static struct mrsh_function_definition *function_definition(
		struct mrsh_parser *state) {
	size_t name_len = peek_name(state, false);
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

	struct mrsh_range name_range;
	char *name = read_token(state, name_len, &name_range);

	struct mrsh_position lparen_pos = state->pos;
	if (!expect_token(state, "(", NULL)) {
		return NULL;
	}

	struct mrsh_position rparen_pos = state->pos;
	if (!expect_token(state, ")", NULL)) {
		return NULL;
	}

	linebreak(state);

	struct mrsh_command *cmd = compound_command(state);
	if (cmd == NULL) {
		parser_set_error(state, "expected a compount command");
		return NULL;
	}

	// TODO: compound_command redirect_list

	struct mrsh_function_definition *fd =
		mrsh_function_definition_create(name, cmd);
	fd->name_range = name_range;
	fd->lparen_pos = lparen_pos;
	fd->rparen_pos = rparen_pos;
	return fd;
}

static struct mrsh_command *compound_command(struct mrsh_parser *state) {
	struct mrsh_brace_group *bg = brace_group(state);
	if (bg != NULL) {
		return &bg->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_subshell *s = subshell(state);
	if (s != NULL) {
		return &s->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_if_clause *ic = if_clause(state);
	if (ic != NULL) {
		return &ic->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_for_clause *fc = for_clause(state);
	if (fc != NULL) {
		return &fc->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_loop_clause *lc = loop_clause(state);
	if (lc != NULL) {
		return &lc->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_case_clause *cc = case_clause(state);
	if (cc != NULL) {
		return &cc->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	struct mrsh_function_definition *fd = function_definition(state);
	if (fd != NULL) {
		return &fd->command;
	} else if (mrsh_parser_error(state, NULL)) {
		return NULL;
	}

	return NULL;
}

static struct mrsh_command *command(struct mrsh_parser *state) {
	apply_aliases(state);

	struct mrsh_command *cmd = compound_command(state);
	if (cmd != NULL || mrsh_parser_error(state, NULL)) {
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
	struct mrsh_range bang_range = {0};
	bool bang = token(state, "!", &bang_range);
	struct mrsh_position bang_pos = bang_range.begin; // can be invalid

	struct mrsh_command *cmd = command(state);
	if (cmd == NULL) {
		return NULL;
	}

	struct mrsh_array commands = {0};
	mrsh_array_add(&commands, cmd);

	while (token(state, "|", NULL)) {
		linebreak(state);
		struct mrsh_command *cmd = command(state);
		if (cmd == NULL) {
			parser_set_error(state, "expected a command");
			goto error_commands;
		}
		mrsh_array_add(&commands, cmd);
	}

	struct mrsh_pipeline *p = mrsh_pipeline_create(&commands, bang);
	p->bang_pos = bang_pos;
	return p;

error_commands:
	for (size_t i = 0; i < commands.len; ++i) {
		mrsh_command_destroy((struct mrsh_command *)commands.data[i]);
	}
	mrsh_array_finish(&commands);
	return NULL;
}

static struct mrsh_and_or_list *and_or(struct mrsh_parser *state) {
	struct mrsh_pipeline *pl = pipeline(state);
	if (pl == NULL) {
		return NULL;
	}

	enum mrsh_binop_type binop_type;
	struct mrsh_range op_range;
	if (operator(state, AND_IF, &op_range)) {
		binop_type = MRSH_BINOP_AND;
	} else if (operator(state, OR_IF, &op_range)) {
		binop_type = MRSH_BINOP_OR;
	} else {
		return &pl->and_or_list;
	}

	linebreak(state);
	struct mrsh_and_or_list *and_or_list = and_or(state);
	if (and_or_list == NULL) {
		mrsh_and_or_list_destroy(&pl->and_or_list);
		parser_set_error(state, "expected an AND-OR list");
		return NULL;
	}

	struct mrsh_binop *binop = mrsh_binop_create(binop_type, &pl->and_or_list, and_or_list);
	binop->op_range = op_range;
	return &binop->and_or_list;
}

static struct mrsh_command_list *list(struct mrsh_parser *state) {
	struct mrsh_and_or_list *and_or_list = and_or(state);
	if (and_or_list == NULL) {
		return NULL;
	}

	struct mrsh_command_list *cmd = mrsh_command_list_create();
	cmd->and_or_list = and_or_list;

	struct mrsh_position separator_pos = state->pos;
	int sep = separator_op(state);
	if (sep == '&') {
		cmd->ampersand = true;
	}
	if (sep >= 0) {
		cmd->separator_pos = separator_pos;
	}

	return cmd;
}

/**
 * Append a new string word to `children` with the contents of `buf`, and reset
 * `buf`.
 */
static void push_buffer_word_string(struct mrsh_array *children,
		struct mrsh_buffer *buf) {
	if (buf->len == 0) {
		return;
	}

	mrsh_buffer_append_char(buf, '\0');

	char *data = mrsh_buffer_steal(buf);
	struct mrsh_word_string *ws = mrsh_word_string_create(data, false);
	mrsh_array_add(children, &ws->word);
}

static struct mrsh_word *here_document_line(struct mrsh_parser *state) {
	struct mrsh_array children = {0};
	struct mrsh_buffer buf = {0};

	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			break;
		}

		if (c == '$') {
			push_buffer_word_string(&children, &buf);
			struct mrsh_word *t = expect_dollar(state);
			if (t == NULL) {
				return NULL;
			}
			mrsh_array_add(&children, t);
			continue;
		}

		if (c == '`') {
			push_buffer_word_string(&children, &buf);
			struct mrsh_word *t = back_quotes(state);
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
		mrsh_buffer_append_char(&buf, c);
	}

	push_buffer_word_string(&children, &buf);
	mrsh_buffer_finish(&buf);

	if (children.len == 1) {
		struct mrsh_word *word = children.data[0];
		mrsh_array_finish(&children); // TODO: don't allocate this array
		return word;
	} else {
		struct mrsh_word_list *wl = mrsh_word_list_create(&children, false);
		return &wl->word;
	}
}

static bool is_word_quoted(struct mrsh_word *word) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		return ws->single_quoted;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->double_quoted) {
			return true;
		}
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word *child = wl->children.data[i];
			if (is_word_quoted(child)) {
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
	bool trim_tabs = redir->op == MRSH_IO_DLESSDASH;
	bool expand_lines = !is_word_quoted(redir->name);

	state->continuation_line = true;

	struct mrsh_buffer buf = {0};
	while (true) {
		buf.len = 0;
		while (true) {
			char c = parser_peek_char(state);
			if (c == '\0' || c == '\n') {
				break;
			}

			mrsh_buffer_append_char(&buf, parser_read_char(state));
		}
		mrsh_buffer_append_char(&buf, '\0');

		const char *line = buf.data;
		if (trim_tabs) {
			while (line[0] == '\t') {
				++line;
			}
		}

		if (strcmp(line, delim) == 0) {
			break;
		}
		if (parser_peek_char(state) == '\0') {
			parser_set_error(state, "unterminated here-document");
			return false;
		}
		read_continuation_line(state);

		struct mrsh_word *word;
		if (expand_lines) {
			struct mrsh_parser *subparser =
				mrsh_parser_with_data(line, strlen(line));
			word = here_document_line(subparser);
			mrsh_parser_destroy(subparser);
		} else {
			struct mrsh_word_string *ws =
				mrsh_word_string_create(strdup(line), true);
			word = &ws->word;
		}

		mrsh_array_add(&redir->here_document, word);
	}
	mrsh_buffer_finish(&buf);

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

			char *delim = mrsh_word_str(redir->name);
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
	struct mrsh_program *prog = mrsh_program_create();
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
	parser_begin(state);

	if (eof(state)) {
		return NULL;
	}

	struct mrsh_program *prog = mrsh_program_create();
	if (prog == NULL) {
		return NULL;
	}

	if (newline(state)) {
		return prog;
	}

	if (!expect_complete_command(state, &prog->body)) {
		goto error;
	}
	if (!eof(state) && !newline(state)) {
		parser_set_error(state, "expected a newline");
		goto error;
	}

	return prog;

error:
	mrsh_program_destroy(prog);

	// Consume the whole line
	while (true) {
		char c = parser_peek_char(state);
		if (c == '\0') {
			break;
		}

		parser_read_char(state);
		if (c == '\n') {
			break;
		}
	}

	state->has_sym = false;

	return NULL;
}

struct mrsh_program *mrsh_parse_program(struct mrsh_parser *state) {
	parser_begin(state);
	return program(state);
}
