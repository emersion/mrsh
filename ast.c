#include <assert.h>
#include <mrsh/buffer.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

bool mrsh_position_valid(const struct mrsh_position *pos) {
	return pos->line > 0;
}

bool mrsh_range_valid(const struct mrsh_range *range) {
	return mrsh_position_valid(&range->begin) &&
		mrsh_position_valid(&range->end);
}

void mrsh_word_destroy(struct mrsh_word *word) {
	if (word == NULL) {
		return;
	}

	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		free(ws->str);
		free(ws);
		return;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		free(wp->name);
		mrsh_word_destroy(wp->arg);
		free(wp);
		return;
	case MRSH_WORD_COMMAND:;
		struct mrsh_word_command *wc = mrsh_word_get_command(word);
		mrsh_program_destroy(wc->program);
		free(wc);
		return;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word *child = wl->children.data[i];
			mrsh_word_destroy(child);
		}
		mrsh_array_finish(&wl->children);
		free(wl);
		return;
	}
	assert(false);
}

void mrsh_io_redirect_destroy(struct mrsh_io_redirect *redir) {
	if (redir == NULL) {
		return;
	}
	mrsh_word_destroy(redir->name);
	for (size_t i = 0; i < redir->here_document.len; ++i) {
		struct mrsh_word *line = redir->here_document.data[i];
		mrsh_word_destroy(line);
	}
	mrsh_array_finish(&redir->here_document);
	free(redir);
}

void mrsh_assignment_destroy(struct mrsh_assignment *assign) {
	if (assign == NULL) {
		return;
	}
	free(assign->name);
	mrsh_word_destroy(assign->value);
	free(assign);
}

void command_list_array_finish(struct mrsh_array *cmds) {
	for (size_t i = 0; i < cmds->len; ++i) {
		struct mrsh_command_list *l = cmds->data[i];
		mrsh_command_list_destroy(l);
	}
	mrsh_array_finish(cmds);
}

void case_item_destroy(struct mrsh_case_item *item) {
	for (size_t j = 0; j < item->patterns.len; ++j) {
		struct mrsh_word *pattern = item->patterns.data[j];
		mrsh_word_destroy(pattern);
	}
	mrsh_array_finish(&item->patterns);
	command_list_array_finish(&item->body);
	free(item);
}

void mrsh_command_destroy(struct mrsh_command *cmd) {
	if (cmd == NULL) {
		return;
	}

	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		mrsh_word_destroy(sc->name);
		for (size_t i = 0; i < sc->arguments.len; ++i) {
			struct mrsh_word *arg = sc->arguments.data[i];
			mrsh_word_destroy(arg);
		}
		mrsh_array_finish(&sc->arguments);
		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
			mrsh_io_redirect_destroy(redir);
		}
		mrsh_array_finish(&sc->io_redirects);
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			mrsh_assignment_destroy(assign);
		}
		mrsh_array_finish(&sc->assignments);
		free(sc);
		return;
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		command_list_array_finish(&bg->body);
		free(bg);
		return;
	case MRSH_SUBSHELL:;
		struct mrsh_subshell *s = mrsh_command_get_subshell(cmd);
		command_list_array_finish(&s->body);
		free(s);
		return;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		command_list_array_finish(&ic->condition);
		command_list_array_finish(&ic->body);
		mrsh_command_destroy(ic->else_part);
		free(ic);
		return;
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		free(fc->name);
		for (size_t i = 0; i < fc->word_list.len; ++i) {
			struct mrsh_word *word = fc->word_list.data[i];
			mrsh_word_destroy(word);
		}
		mrsh_array_finish(&fc->word_list);
		command_list_array_finish(&fc->body);
		free(fc);
		return;
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		command_list_array_finish(&lc->condition);
		command_list_array_finish(&lc->body);
		free(lc);
		return;
	case MRSH_CASE_CLAUSE:;
		struct mrsh_case_clause *cc = mrsh_command_get_case_clause(cmd);
		mrsh_word_destroy(cc->word);
		for (size_t i = 0; i < cc->items.len; ++i) {
			struct mrsh_case_item *item = cc->items.data[i];
			case_item_destroy(item);
		}
		mrsh_array_finish(&cc->items);
		free(cc);
		return;
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fd =
			mrsh_command_get_function_definition(cmd);
		free(fd->name);
		mrsh_command_destroy(fd->body);
		free(fd);
		return;
	}
	assert(0);
}

void mrsh_node_destroy(struct mrsh_node *node) {
	if (node == NULL) {
		return;
	}

	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *p = mrsh_node_get_pipeline(node);
		for (size_t i = 0; i < p->commands.len; ++i) {
			struct mrsh_command *cmd = p->commands.data[i];
			mrsh_command_destroy(cmd);
		}
		mrsh_array_finish(&p->commands);
		free(p);
		return;
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		mrsh_node_destroy(binop->left);
		mrsh_node_destroy(binop->right);
		free(binop);
		return;
	}
	assert(0);
}

void mrsh_command_list_destroy(struct mrsh_command_list *l) {
	if (l == NULL) {
		return;
	}

	mrsh_node_destroy(l->node);
	free(l);
}

void mrsh_program_destroy(struct mrsh_program *prog) {
	if (prog == NULL) {
		return;
	}

	command_list_array_finish(&prog->body);
	free(prog);
}

struct mrsh_word_string *mrsh_word_string_create(char *str,
		bool single_quoted) {
	struct mrsh_word_string *ws = calloc(1, sizeof(struct mrsh_word_string));
	ws->word.type = MRSH_WORD_STRING;
	ws->str = str;
	ws->single_quoted = single_quoted;
	return ws;
}

struct mrsh_word_parameter *mrsh_word_parameter_create(char *name,
		enum mrsh_word_parameter_op op, bool colon, struct mrsh_word *arg) {
	struct mrsh_word_parameter *wp =
		calloc(1, sizeof(struct mrsh_word_parameter));
	wp->word.type = MRSH_WORD_PARAMETER;
	wp->name = name;
	wp->op = op;
	wp->colon = colon;
	wp->arg = arg;
	return wp;
}

struct mrsh_word_command *mrsh_word_command_create(struct mrsh_program *prog,
		bool back_quoted) {
	struct mrsh_word_command *wc =
		calloc(1, sizeof(struct mrsh_word_command));
	wc->word.type = MRSH_WORD_COMMAND;
	wc->program = prog;
	wc->back_quoted = back_quoted;
	return wc;
}

struct mrsh_word_list *mrsh_word_list_create(struct mrsh_array *children,
		bool double_quoted) {
	struct mrsh_word_list *wl = calloc(1, sizeof(struct mrsh_word_list));
	wl->word.type = MRSH_WORD_LIST;
	wl->children = *children;
	wl->double_quoted = double_quoted;
	return wl;
}

struct mrsh_word_string *mrsh_word_get_string(struct mrsh_word *word) {
	assert(word->type == MRSH_WORD_STRING);
	return (struct mrsh_word_string *)word;
}

struct mrsh_word_parameter *mrsh_word_get_parameter(struct mrsh_word *word) {
	assert(word->type == MRSH_WORD_PARAMETER);
	return (struct mrsh_word_parameter *)word;
}

struct mrsh_word_command *mrsh_word_get_command(struct mrsh_word *word) {
	assert(word->type == MRSH_WORD_COMMAND);
	return (struct mrsh_word_command *)word;
}

struct mrsh_word_list *mrsh_word_get_list(struct mrsh_word *word) {
	assert(word->type == MRSH_WORD_LIST);
	return (struct mrsh_word_list *)word;
}

struct mrsh_simple_command *mrsh_simple_command_create(struct mrsh_word *name,
		struct mrsh_array *arguments, struct mrsh_array *io_redirects,
		struct mrsh_array *assignments) {
	struct mrsh_simple_command *cmd =
		calloc(1, sizeof(struct mrsh_simple_command));
	cmd->command.type = MRSH_SIMPLE_COMMAND;
	cmd->name = name;
	cmd->arguments = *arguments;
	cmd->io_redirects = *io_redirects;
	cmd->assignments = *assignments;
	return cmd;
}

struct mrsh_brace_group *mrsh_brace_group_create(struct mrsh_array *body) {
	struct mrsh_brace_group *bg = calloc(1, sizeof(struct mrsh_brace_group));
	bg->command.type = MRSH_BRACE_GROUP;
	bg->body = *body;
	return bg;
}

struct mrsh_subshell *mrsh_subshell_create(struct mrsh_array *body) {
	struct mrsh_subshell *s = calloc(1, sizeof(struct mrsh_subshell));
	s->command.type = MRSH_SUBSHELL;
	s->body = *body;
	return s;
}

struct mrsh_if_clause *mrsh_if_clause_create(struct mrsh_array *condition,
		struct mrsh_array *body, struct mrsh_command *else_part) {
	struct mrsh_if_clause *ic = calloc(1, sizeof(struct mrsh_if_clause));
	ic->command.type = MRSH_IF_CLAUSE;
	ic->condition = *condition;
	ic->body = *body;
	ic->else_part = else_part;
	return ic;
}

struct mrsh_for_clause *mrsh_for_clause_create(char *name, bool in,
		struct mrsh_array *word_list, struct mrsh_array *body) {
	struct mrsh_for_clause *fc = calloc(1, sizeof(struct mrsh_for_clause));
	fc->command.type = MRSH_FOR_CLAUSE;
	fc->name = name;
	fc->in = in;
	fc->word_list = *word_list;
	fc->body = *body;
	return fc;
}

struct mrsh_loop_clause *mrsh_loop_clause_create(enum mrsh_loop_type type,
		struct mrsh_array *condition, struct mrsh_array *body) {
	struct mrsh_loop_clause *lc = calloc(1, sizeof(struct mrsh_loop_clause));
	lc->command.type = MRSH_LOOP_CLAUSE;
	lc->type = type;
	lc->condition = *condition;
	lc->body = *body;
	return lc;
}

struct mrsh_case_clause *mrsh_case_clause_create(struct mrsh_word *word,
		struct mrsh_array *items) {
	struct mrsh_case_clause *cc = calloc(1, sizeof(struct mrsh_case_clause));
	cc->command.type = MRSH_CASE_CLAUSE;
	cc->word = word;
	cc->items = *items;
	return cc;
}

struct mrsh_function_definition *mrsh_function_definition_create(char *name,
		struct mrsh_command *body) {
	struct mrsh_function_definition *fd =
		calloc(1, sizeof(struct mrsh_function_definition));
	fd->command.type = MRSH_FUNCTION_DEFINITION;
	fd->name = name;
	fd->body = body;
	return fd;
}

struct mrsh_simple_command *mrsh_command_get_simple_command(
		struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_SIMPLE_COMMAND);
	return (struct mrsh_simple_command *)cmd;
}

struct mrsh_brace_group *mrsh_command_get_brace_group(
		struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_BRACE_GROUP);
	return (struct mrsh_brace_group *)cmd;
}

struct mrsh_subshell *mrsh_command_get_subshell(struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_SUBSHELL);
	return (struct mrsh_subshell *)cmd;
}

struct mrsh_if_clause *mrsh_command_get_if_clause(struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_IF_CLAUSE);
	return (struct mrsh_if_clause *)cmd;
}

struct mrsh_for_clause *mrsh_command_get_for_clause(struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_FOR_CLAUSE);
	return (struct mrsh_for_clause *)cmd;
}

struct mrsh_loop_clause *mrsh_command_get_loop_clause(
		struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_LOOP_CLAUSE);
	return (struct mrsh_loop_clause *)cmd;
}

struct mrsh_case_clause *mrsh_command_get_case_clause(
		struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_CASE_CLAUSE);
	return (struct mrsh_case_clause *)cmd;
}

struct mrsh_function_definition *mrsh_command_get_function_definition(
		struct mrsh_command *cmd) {
	assert(cmd->type == MRSH_FUNCTION_DEFINITION);
	return (struct mrsh_function_definition *)cmd;
}

struct mrsh_pipeline *mrsh_pipeline_create(struct mrsh_array *commands,
		bool bang) {
	struct mrsh_pipeline *pl = calloc(1, sizeof(struct mrsh_pipeline));
	pl->node.type = MRSH_NODE_PIPELINE;
	pl->commands = *commands;
	pl->bang = bang;
	return pl;
}

struct mrsh_binop *mrsh_binop_create(enum mrsh_binop_type type,
		struct mrsh_node *left, struct mrsh_node *right) {
	struct mrsh_binop *binop = calloc(1, sizeof(struct mrsh_binop));
	binop->node.type = MRSH_NODE_BINOP;
	binop->type = type;
	binop->left = left;
	binop->right = right;
	return binop;
}

struct mrsh_pipeline *mrsh_node_get_pipeline(struct mrsh_node *node) {
	assert(node->type == MRSH_NODE_PIPELINE);
	return (struct mrsh_pipeline *)node;
}

struct mrsh_binop *mrsh_node_get_binop(struct mrsh_node *node) {
	assert(node->type == MRSH_NODE_BINOP);
	return (struct mrsh_binop *)node;
}

static void position_next(struct mrsh_position *dst,
		const struct mrsh_position *src) {
	*dst = *src;
	++dst->offset;
	++dst->column;
}

void mrsh_word_range(struct mrsh_word *word, struct mrsh_position *begin,
		struct mrsh_position *end) {
	if (begin == NULL && end == NULL) {
		return;
	}

	struct mrsh_position _begin, _end;
	if (begin == NULL) {
		begin = &_begin;
	}
	if (end == NULL) {
		end = &_end;
	}

	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		*begin = ws->range.begin;
		*end = ws->range.end;
		return;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		*begin = wp->dollar_pos;
		if (mrsh_position_valid(&wp->rbrace_pos)) {
			position_next(end, &wp->rbrace_pos);
		} else {
			*end = wp->name_range.end;
		}
		return;
	case MRSH_WORD_COMMAND:;
		struct mrsh_word_command *wc = mrsh_word_get_command(word);
		*begin = wc->range.begin;
		*end = wc->range.end;
		return;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->children.len == 0) {
			*begin = *end = (struct mrsh_position){0};
		} else {
			struct mrsh_word *first = wl->children.data[0];
			struct mrsh_word *last = wl->children.data[wl->children.len - 1];
			mrsh_word_range(first, begin, NULL);
			mrsh_word_range(last, NULL, end);
		}
		return;
	}
	assert(false);
}

void mrsh_command_range(struct mrsh_command *cmd, struct mrsh_position *begin,
		struct mrsh_position *end) {
	if (begin == NULL && end == NULL) {
		return;
	}

	struct mrsh_position _begin, _end;
	if (begin == NULL) {
		begin = &_begin;
	}
	if (end == NULL) {
		end = &_end;
	}

	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);

		if (sc->name != NULL) {
			mrsh_word_range(sc->name, begin, end);
		} else {
			assert(sc->assignments.len > 0);
			struct mrsh_assignment *first = sc->assignments.data[0];
			*begin = first->name_range.begin;
			*end = *begin; // That's a lie, but it'll be fixed by the code below
		}

		struct mrsh_position maybe_end;
		for (size_t i = 0; i < sc->arguments.len; ++i) {
			struct mrsh_word *arg = sc->arguments.data[i];
			mrsh_word_range(arg, NULL, &maybe_end);
			if (maybe_end.offset > end->offset) {
				*end = maybe_end;
			}
		}
		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
			mrsh_word_range(redir->name, NULL, &maybe_end);
			if (maybe_end.offset > end->offset) {
				*end = maybe_end;
			}
		}
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			mrsh_word_range(assign->value, NULL, &maybe_end);
			if (maybe_end.offset > end->offset) {
				*end = maybe_end;
			}
		}
		return;
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		*begin = bg->lbrace_pos;
		position_next(end, &bg->rbrace_pos);
		return;
	case MRSH_SUBSHELL:;
		struct mrsh_subshell *s = mrsh_command_get_subshell(cmd);
		*begin = s->lparen_pos;
		position_next(end, &s->rparen_pos);
		return;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		*begin = ic->if_range.begin;
		*end = ic->fi_range.end;
		return;
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		*begin = fc->for_range.begin;
		*end = fc->done_range.end;
		return;
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		*begin = lc->while_until_range.begin;
		*end = lc->done_range.end;
		return;
	case MRSH_CASE_CLAUSE:;
		struct mrsh_case_clause *cc = mrsh_command_get_case_clause(cmd);
		*begin = cc->case_range.begin;
		*end = cc->esac_range.end;
		return;
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fd =
			mrsh_command_get_function_definition(cmd);
		*begin = fd->name_range.begin;
		mrsh_command_range(fd->body, NULL, end);
	}
	assert(false);
}

static void word_str(struct mrsh_word *word, struct mrsh_buffer *buf) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		mrsh_buffer_append(buf, ws->str, strlen(ws->str));
		return;
	case MRSH_WORD_PARAMETER:
	case MRSH_WORD_COMMAND:
		assert(false);
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word *child = wl->children.data[i];
			word_str(child, buf);
		}
		return;
	}
	assert(false);
}

char *mrsh_word_str(struct mrsh_word *word) {
	struct mrsh_buffer buf = {0};
	word_str(word, &buf);
	mrsh_buffer_append_char(&buf, '\0');
	return mrsh_buffer_steal(&buf);
}
