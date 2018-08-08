#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "shell.h"

void expand_tilde(struct mrsh_state *state, char **str_ptr) {
	char *str = *str_ptr;
	if (str[0] != '~') {
		return;
	}

	const char *end = str + strlen(str);
	const char *slash = strchr(str, '/');
	if (slash == NULL) {
		slash = end;
	}

	char *name = NULL;
	if (slash > str + 1) {
		name = strndup(str + 1, slash - str - 1);
	}

	const char *dir = NULL;
	if (name == NULL) {
		dir = mrsh_hashtable_get(&state->variables, "HOME");
	} else {
		struct passwd *pw = getpwnam(name);
		if (pw != NULL) {
			dir = pw->pw_dir;
		}
	}
	free(name);

	if (dir == NULL) {
		return;
	}

	size_t dir_len = strlen(dir);
	size_t trailing_len = end - slash;
	char *expanded = malloc(dir_len + trailing_len + 1);
	if (expanded == NULL) {
		return;
	}
	memcpy(expanded, dir, dir_len);
	memcpy(expanded + dir_len, slash, trailing_len);
	expanded[dir_len + trailing_len] = '\0';

	free(str);
	*str_ptr = expanded;
}

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
