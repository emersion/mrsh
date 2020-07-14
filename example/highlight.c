#include <assert.h>
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READ_SIZE 4096
#define FORMAT_STACK_SIZE 64

enum format {
	FORMAT_RESET = 0,
	FORMAT_GREEN = 32,
	FORMAT_YELLOW = 33,
	FORMAT_BLUE = 34,
	FORMAT_CYAN = 36,
	FORMAT_DEFAULT = 39,
	FORMAT_LIGHT_BLUE = 94,
};

struct highlight_state {
	const char *buf;
	size_t offset;

	enum format fmt_stack[FORMAT_STACK_SIZE];
	size_t fmt_stack_len;
};

static void highlight(struct highlight_state *state, struct mrsh_position *pos,
		enum format fmt) {
	assert(pos->offset >= state->offset);

	fwrite(&state->buf[state->offset], sizeof(char),
		pos->offset - state->offset, stdout);
	state->offset = pos->offset;

	if (fmt == FORMAT_RESET) {
		assert(state->fmt_stack_len > 0);
		--state->fmt_stack_len;
		if (state->fmt_stack_len > 0) {
			fmt = state->fmt_stack[state->fmt_stack_len - 1];
		}
	} else {
		if (state->fmt_stack_len >= FORMAT_STACK_SIZE) {
			fprintf(stderr, "format stack overflow\n");
			exit(1);
		}
		state->fmt_stack[state->fmt_stack_len] = fmt;
		++state->fmt_stack_len;
	}

	fprintf(stdout, "%c[%dm", 0x1B, fmt);
}

static void highlight_str(struct highlight_state *state,
		struct mrsh_range *range, enum format fmt) {
	highlight(state, &range->begin, fmt);
	highlight(state, &range->end, FORMAT_RESET);
}

static void highlight_char(struct highlight_state *state,
		struct mrsh_position *pos, enum format fmt) {
	struct mrsh_position next = { .offset = pos->offset + 1 };
	highlight(state, pos, fmt);
	highlight(state, &next, FORMAT_RESET);
}

static void highlight_word(struct highlight_state *state,
		struct mrsh_word *word, bool cmd_name, bool quoted) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (!quoted) {
			enum format fmt = FORMAT_CYAN;
			if (ws->single_quoted) {
				fmt = FORMAT_YELLOW;
			} else if (cmd_name) {
				fmt = FORMAT_BLUE;
			}
			highlight_str(state, &ws->range, fmt);
		}
		break;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		highlight(state, &wp->dollar_pos, FORMAT_CYAN);
		highlight_char(state, &wp->dollar_pos, FORMAT_GREEN);
		if (mrsh_position_valid(&wp->lbrace_pos)) {
			highlight_char(state, &wp->lbrace_pos, FORMAT_GREEN);
		}
		if (mrsh_range_valid(&wp->op_range)) {
			highlight_str(state, &wp->op_range, FORMAT_GREEN);
		}
		if (wp->arg != NULL) {
			highlight_word(state, wp->arg, false, false);
		}
		struct mrsh_position end = {0};
		if (mrsh_position_valid(&wp->rbrace_pos)) {
			highlight_char(state, &wp->rbrace_pos, FORMAT_GREEN);
			end.offset = wp->rbrace_pos.offset + 1;
		} else {
			end = wp->name_range.end;
		}
		highlight(state, &end, FORMAT_RESET);
		break;
	case MRSH_WORD_COMMAND:;
		struct mrsh_word_command *wc = mrsh_word_get_command(word);
		if (wc->back_quoted) {
			highlight_char(state, &wc->range.begin, FORMAT_GREEN);
		}
		// TODO: highlight inside
		if (wc->back_quoted) {
			struct mrsh_position rquote = {
				.offset = wc->range.end.offset - 1,
			};
			highlight_char(state, &rquote, FORMAT_GREEN);
		}
		break;
	case MRSH_WORD_ARITHMETIC:
		abort(); // TODO
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->children.len == 0) {
			break;
		}

		if (wl->double_quoted) {
			highlight(state, &wl->lquote_pos, FORMAT_YELLOW);
		}

		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word *child = wl->children.data[i];
			highlight_word(state, child, cmd_name, wl->double_quoted);
		}

		if (wl->double_quoted) {
			struct mrsh_position end = { .offset = wl->rquote_pos.offset + 1 };
			highlight(state, &end, FORMAT_RESET);
		}
		break;
	}
}

static void highlight_simple_command(struct highlight_state *state,
		struct mrsh_simple_command *cmd) {
	if (cmd->name != NULL) {
		highlight_word(state, cmd->name, true, false);
	}

	for (size_t i = 0; i < cmd->arguments.len; ++i) {
		struct mrsh_word *arg = cmd->arguments.data[i];
		highlight_word(state, arg, false, false);
	}

	// TODO: cmd->io_redirects, cmd->assignments
}

static void highlight_command_list_array(struct highlight_state *state,
	struct mrsh_array *array);

static void highlight_command(struct highlight_state *state,
		struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		highlight_simple_command(state, sc);
		break;
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		highlight_char(state, &bg->lbrace_pos, FORMAT_GREEN);
		highlight_command_list_array(state, &bg->body);
		highlight_char(state, &bg->rbrace_pos, FORMAT_GREEN);
		break;
	case MRSH_SUBSHELL:;
		struct mrsh_subshell *s = mrsh_command_get_subshell(cmd);
		highlight_char(state, &s->lparen_pos, FORMAT_GREEN);
		highlight_command_list_array(state, &s->body);
		highlight_char(state, &s->rparen_pos, FORMAT_GREEN);
		break;
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		highlight_str(state, &ic->if_range, FORMAT_BLUE);
		highlight_command_list_array(state, &ic->condition);
		highlight_str(state, &ic->then_range, FORMAT_BLUE);
		highlight_command_list_array(state, &ic->body);
		if (ic->else_part != NULL) {
			if (mrsh_range_valid(&ic->else_range)) {
				highlight_str(state, &ic->else_range, FORMAT_BLUE);
			}
			highlight_command(state, ic->else_part);
		}
		if (mrsh_range_valid(&ic->fi_range)) {
			highlight_str(state, &ic->fi_range, FORMAT_BLUE);
		}
		break;
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		highlight_str(state, &fc->for_range, FORMAT_BLUE);
		highlight_str(state, &fc->name_range, FORMAT_CYAN);
		if (mrsh_range_valid(&fc->in_range)) {
			highlight_str(state, &fc->in_range, FORMAT_BLUE);
		}
		for (size_t i = 0; i < fc->word_list.len; ++i) {
			struct mrsh_word *word = fc->word_list.data[i];
			highlight_word(state, word, false, false);
		}
		highlight_str(state, &fc->do_range, FORMAT_BLUE);
		highlight_command_list_array(state, &fc->body);
		highlight_str(state, &fc->done_range, FORMAT_BLUE);
		break;
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		highlight_str(state, &lc->while_until_range, FORMAT_BLUE);
		highlight_command_list_array(state, &lc->condition);
		highlight_str(state, &lc->do_range, FORMAT_BLUE);
		highlight_command_list_array(state, &lc->body);
		highlight_str(state, &lc->done_range, FORMAT_BLUE);
		break;
	case MRSH_CASE_CLAUSE:;
		struct mrsh_case_clause *cc = mrsh_command_get_case_clause(cmd);
		highlight_str(state, &cc->case_range, FORMAT_BLUE);
		highlight_word(state, cc->word, false, false);
		highlight_str(state, &cc->in_range, FORMAT_BLUE);
		for (size_t i = 0; i < cc->items.len; ++i) {
			struct mrsh_case_item *item = cc->items.data[i];
			if (mrsh_position_valid(&item->lparen_pos)) {
				highlight_char(state, &item->lparen_pos, FORMAT_GREEN);
			}
			for (size_t j = 0; j < item->patterns.len; ++j) {
				struct mrsh_word *pattern = item->patterns.data[j];
				highlight_word(state, pattern, false, false);
			}
			highlight_char(state, &item->rparen_pos, FORMAT_GREEN);
			highlight_command_list_array(state, &item->body);
			if (mrsh_range_valid(&item->dsemi_range)) {
				highlight_str(state, &item->dsemi_range, FORMAT_GREEN);
			}
		}
		highlight_str(state, &cc->esac_range, FORMAT_BLUE);
		break;
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fd =
			mrsh_command_get_function_definition(cmd);
		highlight_str(state, &fd->name_range, FORMAT_BLUE);
		highlight_char(state, &fd->lparen_pos, FORMAT_GREEN);
		highlight_char(state, &fd->rparen_pos, FORMAT_GREEN);
		highlight_command(state, fd->body);
		break;
	}
}

static void highlight_and_or_list(struct highlight_state *state,
		struct mrsh_and_or_list *and_or_list) {
	switch (and_or_list->type) {
	case MRSH_AND_OR_LIST_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_and_or_list_get_pipeline(and_or_list);
		if (mrsh_position_valid(&pl->bang_pos)) {
			highlight_char(state, &pl->bang_pos, FORMAT_GREEN);
		}
		for (size_t i = 0; i < pl->commands.len; ++i) {
			struct mrsh_command *cmd = pl->commands.data[i];
			highlight_command(state, cmd);
		}
		break;
	case MRSH_AND_OR_LIST_BINOP:;
		struct mrsh_binop *binop = mrsh_and_or_list_get_binop(and_or_list);
		highlight_and_or_list(state, binop->left);
		highlight_str(state, &binop->op_range, FORMAT_GREEN);
		highlight_and_or_list(state, binop->right);
		break;
	}
}

static void highlight_command_list(struct highlight_state *state,
		struct mrsh_command_list *list) {
	highlight_and_or_list(state, list->and_or_list);
	if (mrsh_position_valid(&list->separator_pos)) {
		highlight_char(state, &list->separator_pos, FORMAT_GREEN);
	}
}

static void highlight_command_list_array(struct highlight_state *state,
		struct mrsh_array *array) {
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *l = array->data[i];
		highlight_command_list(state, l);
	}
}

static void highlight_program(struct highlight_state *state,
		struct mrsh_program *prog) {
	highlight_command_list_array(state, &prog->body);
}

int main(int argc, char *argv[]) {
	struct mrsh_buffer buf = {0};
	while (true) {
		char *dst = mrsh_buffer_reserve(&buf, READ_SIZE);
		ssize_t n_read = read(STDIN_FILENO, dst, READ_SIZE);
		if (n_read < 0) {
			perror("read");
			return 1;
		} else if (n_read == 0) {
			break;
		}
		buf.len += n_read;
	}

	struct mrsh_parser *parser = mrsh_parser_with_data(buf.data, buf.len);
	struct mrsh_program *prog = mrsh_parse_program(parser);
	const char *err_msg = mrsh_parser_error(parser, NULL);
	if (err_msg != NULL) {
		fprintf(stderr, "failed to parse script: %s\n", err_msg);
		return 1;
	}
	if (prog == NULL) {
		return 0;
	}
	mrsh_parser_destroy(parser);

	struct highlight_state state = {
		.buf = buf.data,
	};

	struct mrsh_position begin = { .offset = 0 };
	highlight(&state, &begin, FORMAT_DEFAULT);

	highlight_program(&state, prog);

	struct mrsh_position end = { .offset = buf.len };
	highlight(&state, &end, FORMAT_RESET);
	assert(state.fmt_stack_len == 0);

	mrsh_buffer_finish(&buf);
	return 0;
}
