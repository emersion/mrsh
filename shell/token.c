#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "buffer.h"
#include "shell.h"

struct split_fields_data {
	const char *ifs, *ifs_non_space;
	bool in_ifs, in_ifs_non_space;
};

static void _split_fields(struct mrsh_array *fields, struct buffer *buf,
		struct mrsh_token *token, bool double_quoted,
		struct split_fields_data *data) {
	switch (token->type) {
	case MRSH_TOKEN_STRING:;
		struct mrsh_token_string *ts = mrsh_token_get_string(token);
		assert(ts != NULL);

		if (double_quoted) {
			buffer_append(buf, ts->str, strlen(ts->str));
			data->in_ifs = data->in_ifs_non_space = false;
			return;
		}

		size_t len = strlen(ts->str);
		for (size_t i = 0; i < len; ++i) {
			char c = ts->str[i];
			if (strchr(data->ifs, c) == NULL) {
				buffer_append_char(buf, c);
				data->in_ifs = data->in_ifs_non_space = false;
				continue;
			}

			bool is_ifs_non_space = strchr(data->ifs_non_space, c) != NULL;
			if (!data->in_ifs || (is_ifs_non_space && data->in_ifs_non_space)) {
				buffer_append_char(buf, '\0');
				char *str = buffer_steal(buf);
				mrsh_array_add(fields, str);
				data->in_ifs = true;
			} else if (is_ifs_non_space) {
				data->in_ifs_non_space = true;
			}
		}
		break;
	case MRSH_TOKEN_LIST:;
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token *child = tl->children.data[i];
			_split_fields(fields, buf, child,
				double_quoted || tl->double_quoted, data);
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
	struct split_fields_data data = {
		.ifs = ifs,
		.ifs_non_space = ifs_non_space,
		.in_ifs = true,
	};
	_split_fields(fields, &buf, token, false, &data);
	if (!data.in_ifs) {
		buffer_append_char(&buf, '\0');
		char *str = buffer_steal(&buf);
		mrsh_array_add(fields, str);
	}
	buffer_finish(&buf);

	free(ifs_non_space);
}
