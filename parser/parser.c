#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <mrsh/buffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "parser.h"

#define READ_SIZE 4096

// Keep sorted from the longest to the shortest
const struct symbol operators[] = {
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

const size_t operators_len = sizeof(operators) / sizeof(operators[0]);
const size_t operators_max_str_len = 3;

const char *keywords[] = {
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

const size_t keywords_len = sizeof(keywords) / sizeof(keywords[0]);

static struct mrsh_parser *parser_create(void) {
	struct mrsh_parser *parser = calloc(1, sizeof(struct mrsh_parser));
	parser->fd = -1;
	parser->pos.line = parser->pos.column = 1;
	return parser;
}

struct mrsh_parser *mrsh_parser_with_fd(int fd) {
	struct mrsh_parser *parser = parser_create();
	parser->fd = fd;
	return parser;
}

struct mrsh_parser *mrsh_parser_with_data(const char *buf, size_t len) {
	struct mrsh_parser *parser = parser_create();
	mrsh_buffer_append(&parser->buf, buf, len);
	mrsh_buffer_append_char(&parser->buf, '\0');
	return parser;
}

struct mrsh_parser *mrsh_parser_with_buffer(struct mrsh_buffer *buf) {
	struct mrsh_parser *parser = parser_create();
	parser->in_buf = buf;
	return parser;
}

void mrsh_parser_destroy(struct mrsh_parser *parser) {
	if (parser == NULL) {
		return;
	}
	mrsh_buffer_finish(&parser->buf);
	free(parser->error.msg);
	free(parser);
}

static ssize_t parser_peek_fd(struct mrsh_parser *parser, size_t size) {
	assert(parser->fd >= 0);

	size_t n_read = 0;
	while (n_read < size) {
		char *dst = mrsh_buffer_reserve(&parser->buf, READ_SIZE);

		errno = 0;
		ssize_t n = read(parser->fd, dst, READ_SIZE);
		if (n < 0 && errno == EINTR) {
			continue;
		} else if (n < 0) {
			return -1;
		} else if (n == 0) {
			break;
		}

		parser->buf.len += n;
		n_read += n;
	}

	return n_read;
}

static ssize_t parser_peek_buffer(struct mrsh_parser *parser, size_t size) {
	assert(parser->in_buf != NULL);

	size_t n_read = parser->in_buf->len;
	if (n_read == 0) {
		return 0;
	}

	if (parser->buf.len == 0) {
		// Move data from one buffer to the other
		mrsh_buffer_finish(&parser->buf);
		memcpy(&parser->buf, parser->in_buf, sizeof(struct mrsh_buffer));
		memset(parser->in_buf, 0, sizeof(struct mrsh_buffer));
	} else {
		mrsh_buffer_append(&parser->buf, parser->in_buf->data, n_read);
		parser->in_buf->len = 0;
	}

	return n_read;
}

size_t parser_peek(struct mrsh_parser *parser, char *buf, size_t size) {
	if (size > parser->buf.len) {
		size_t n_more = size - parser->buf.len;

		ssize_t n_read;
		if (parser->fd >= 0) {
			n_read = parser_peek_fd(parser, n_more);
		} else if (parser->in_buf != NULL) {
			n_read = parser_peek_buffer(parser, n_more);
		} else {
			n_read = 0;
		}

		if (n_read < 0) {
			parser_set_error(parser, "failed to read");
			return 0; // TODO: better error handling
		}

		if ((size_t)n_read < n_more) {
			if (!parser->eof) {
				mrsh_buffer_append_char(&parser->buf, '\0');
				parser->eof = true;
			}
			size = parser->buf.len;
		}
	}

	if (buf != NULL) {
		memcpy(buf, parser->buf.data, size);
	}
	return size;
}

char parser_peek_char(struct mrsh_parser *parser) {
	char c = '\0';
	parser_peek(parser, &c, sizeof(char));
	return c;
}

size_t parser_read(struct mrsh_parser *parser, char *buf, size_t size) {
	size_t n = parser_peek(parser, buf, size);
	if (n > 0) {
		for (size_t i = 0; i < n; ++i) {
			assert(parser->buf.data[i] != '\0');
			++parser->pos.offset;
			if (parser->buf.data[i] == '\n') {
				++parser->pos.line;
				parser->pos.column = 1;
			} else {
				++parser->pos.column;
			}
		}
		memmove(parser->buf.data, parser->buf.data + n, parser->buf.len - n);
		parser->buf.len -= n;

		parser->continuation_line = false;
	}
	return n;
}

char parser_read_char(struct mrsh_parser *parser) {
	char c = '\0';
	parser_read(parser, &c, sizeof(char));
	return c;
}

void read_continuation_line(struct mrsh_parser *parser) {
	char c = parser_read_char(parser);
	assert(c == '\n');
	parser->continuation_line = true;
}

bool is_operator_start(char c) {
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

void parser_set_error(struct mrsh_parser *parser, const char *msg) {
	if (msg != NULL) {
		if (parser->error.msg != NULL) {
			return;
		}

		parser->here_documents.len = 0;
		parser->error.pos = parser->pos;
		parser->error.msg = strdup(msg);
	} else {
		free(parser->error.msg);

		parser->error.pos = (struct mrsh_position){0};
		parser->error.msg = NULL;
	}
}

const char *mrsh_parser_error(struct mrsh_parser *parser,
		struct mrsh_position *pos) {
	if (pos != NULL) {
		*pos = parser->error.pos;
	}
	return parser->error.msg;
}

void parser_begin(struct mrsh_parser *parser) {
	parser_set_error(parser, NULL);
	parser->continuation_line = false;
}

// See section 2.3 Token Recognition
static void next_symbol(struct mrsh_parser *parser) {
	parser->has_sym = true;

	char c = parser_peek_char(parser);

	if (c == '\0') {
		parser->sym = EOF_TOKEN;
		return;
	}
	if (c == '\n') {
		parser->sym = NEWLINE;
		return;
	}

	if (is_operator_start(c)) {
		for (size_t i = 0; i < operators_len; ++i) {
			const char *str = operators[i].str;

			size_t j;
			for (j = 0; str[j] != '\0'; ++j) {
				size_t n = j + 1;
				size_t n_read = parser_peek(parser, NULL, n);
				if (n != n_read || parser->buf.data[j] != str[j]) {
					break;
				}
			}

			if (str[j] == '\0') {
				parser->sym = operators[i].name;
				return;
			}
		}
	}

	if (isblank(c)) {
		parser_read_char(parser);
		next_symbol(parser);
		return;
	}

	if (c == '#') {
		while (true) {
			char c = parser_peek_char(parser);
			if (c == '\0' || c == '\n') {
				break;
			}
			parser_read_char(parser);
		}
		next_symbol(parser);
		return;
	}

	parser->sym = TOKEN;
}

enum symbol_name get_symbol(struct mrsh_parser *parser) {
	if (!parser->has_sym) {
		next_symbol(parser);
	}
	return parser->sym;
}

void consume_symbol(struct mrsh_parser *parser) {
	parser->has_sym = false;
}

bool symbol(struct mrsh_parser *parser, enum symbol_name sym) {
	return get_symbol(parser) == sym;
}

bool eof(struct mrsh_parser *parser) {
	return symbol(parser, EOF_TOKEN);
}

bool newline(struct mrsh_parser *parser) {
	if (!symbol(parser, NEWLINE)) {
		return false;
	}
	char c = parser_read_char(parser);
	assert(c == '\n');
	consume_symbol(parser);
	return true;
}

void linebreak(struct mrsh_parser *parser) {
	while (newline(parser)) {
		// This space is intentionally left blank
	}
}

bool newline_list(struct mrsh_parser *parser) {
	if (!newline(parser)) {
		return false;
	}

	linebreak(parser);
	return true;
}

bool mrsh_parser_eof(struct mrsh_parser *parser) {
	return parser->has_sym && parser->sym == EOF_TOKEN;
}

void mrsh_parser_set_alias_func(struct mrsh_parser *parser,
		mrsh_parser_alias_func alias, void *user_data) {
	parser->alias = alias;
	parser->alias_user_data = user_data;
}

bool mrsh_parser_continuation_line(struct mrsh_parser *parser) {
	return parser->continuation_line;
}

void mrsh_parser_reset(struct mrsh_parser *parser) {
	parser->buf.len = 0;
	parser->has_sym = false;
	parser->pos = (struct mrsh_position){0};
}
