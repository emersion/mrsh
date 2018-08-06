#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "buffer.h"
#include "shell.h"

static void _split_fields(struct mrsh_array *fields, struct buffer *buf,
		struct mrsh_token *token, const char *ifs, const char *ifs_non_space,
		bool double_quoted) {
	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);

		if (double_quoted) {
			buffer_append(buf, ts->str, strlen(ts->str));
			return;
		}

		const char *cur = ts->str;
		const char *next = cur;
		while (true) {
			next = strpbrk(cur, ifs);
			if (next == NULL) {
				buffer_append(buf, cur, strlen(cur));
				break;
			}

			buffer_append(buf, cur, next - cur);

			// TODO: ifs_non_space

			if (buf->len > 0) {
				buffer_append_char(buf, '\0');
				char *str = buffer_steal(buf);
				mrsh_array_add(fields, str);
			}

			cur = next + 1;
		}
		break;
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			_split_fields(fields, buf, child, ifs, ifs_non_space,
				double_quoted || tl->double_quoted);
		}
		break;
	default:
		assert(false);
	}
}

void split_fields(struct mrsh_array *fields, struct mrsh_token *token,
		const char *ifs) {
	if (ifs == NULL) {
		ifs = " \t\n";
	} else if (ifs[0] == '\0') {
		char *str = mrsh_token_str(token);
		mrsh_array_add(fields, str);
		return;
	}

	size_t ifs_len = strlen(ifs);
	char *ifs_non_space = calloc(ifs_len, sizeof(char));
	size_t ifs_non_space_len = 0;
	for (size_t i = 0; i < ifs_len; ++i) {
		if (!isspace(ifs[i])) {
			ifs_non_space[ifs_non_space_len++] = ifs[i];
		}
	}

	struct buffer buf = {0};
	_split_fields(fields, &buf, token, ifs, ifs_non_space, false);
	buffer_append_char(&buf, '\0');
	char *str = buffer_steal(&buf);
	mrsh_array_add(fields, str);
	buffer_finish(&buf);

	free(ifs_non_space);
}
