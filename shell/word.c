#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <glob.h>
#include <mrsh/buffer.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "shell/shell.h"
#include "shell/word.h"

static bool is_logname_char(char c) {
	// See https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_282
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static ssize_t expand_tilde_at(struct mrsh_state *state, const char *str,
		bool last, char **expanded_ptr) {
	if (str[0] != '~') {
		return -1;
	}

	const char *cur;
	for (cur = str + 1; cur[0] != '\0' && cur[0] != '/'; cur++) {
		if (!is_logname_char(cur[0])) {
			return -1;
		}
	}
	if (cur[0] == '\0' && !last) {
		return -1;
	}
	const char *slash = cur;

	char *name = NULL;
	if (slash > str + 1) {
		name = strndup(str + 1, slash - str - 1);
	}

	const char *dir = NULL;
	if (name == NULL) {
		dir = mrsh_env_get(state, "HOME", NULL);
	} else {
		struct passwd *pw = getpwnam(name);
		if (pw != NULL) {
			dir = pw->pw_dir;
		}
	}
	free(name);

	if (dir == NULL) {
		return -1;
	}

	*expanded_ptr = strdup(dir);
	return slash - str;
}

static void _expand_tilde(struct mrsh_state *state, struct mrsh_word **word_ptr,
		bool assignment, bool first, bool last) {
	struct mrsh_word *word = *word_ptr;
	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (ws->single_quoted) {
			break;
		}

		struct mrsh_array words = {0};

		const char *str = ws->str;
		if (first) {
			char *expanded;
			ssize_t offset = expand_tilde_at(state, str, last, &expanded);
			if (offset >= 0) {
				mrsh_array_add(&words,
					mrsh_word_string_create(expanded, true));
				str += offset;
			}
		}

		if (assignment) {
			while (true) {
				const char *colon = strchr(str, ':');
				if (colon == NULL) {
					break;
				}

				char *slice = strndup(str, colon - str + 1);
				mrsh_array_add(&words,
					mrsh_word_string_create(slice, false));

				str = colon + 1;

				char *expanded;
				ssize_t offset = expand_tilde_at(state, str, last, &expanded);
				if (offset >= 0) {
					mrsh_array_add(&words,
						mrsh_word_string_create(expanded, true));
					str += offset;
				}
			}
		}

		if (words.len > 0) {
			char *trailing = strdup(str);
			mrsh_array_add(&words,
				mrsh_word_string_create(trailing, false));

			struct mrsh_word_list *wl = mrsh_word_list_create(&words, false);
			*word_ptr = &wl->word;
			mrsh_word_destroy(word);
		}
		break;
	case MRSH_WORD_LIST:;
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->double_quoted) {
			break;
		}
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word **child_ptr =
				(struct mrsh_word **)&wl->children.data[i];
			_expand_tilde(state, child_ptr, assignment, first && i == 0,
				last && i == wl->children.len - 1);
		}
		break;
	default:
		break;
	}
}

void expand_tilde(struct mrsh_state *state, struct mrsh_word **word_ptr,
		bool assignment) {
	_expand_tilde(state, word_ptr, assignment, true, true);
}

struct split_fields_data {
	struct mrsh_array *fields;
	struct mrsh_word_list *cur_field;
	const char *ifs, *ifs_non_space;
	bool in_ifs, in_ifs_non_space;
};

static void add_to_cur_field(struct split_fields_data *data,
		struct mrsh_word *word) {
	if (data->cur_field == NULL) {
		data->cur_field = mrsh_word_list_create(NULL, false);
		mrsh_array_add(data->fields, data->cur_field);
	}
	mrsh_array_add(&data->cur_field->children, word);
}

static void _split_fields(struct split_fields_data *data,
		const struct mrsh_word *word) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		const struct mrsh_word_string *ws = mrsh_word_get_string(word);

		if (ws->single_quoted || !ws->split_fields) {
			add_to_cur_field(data, mrsh_word_copy(word));
			data->in_ifs = data->in_ifs_non_space = false;
			return;
		}

		struct mrsh_buffer buf = {0};
		size_t len = strlen(ws->str);
		for (size_t i = 0; i < len; ++i) {
			char c = ws->str[i];
			if (strchr(data->ifs, c) == NULL) {
				mrsh_buffer_append_char(&buf, c);
				data->in_ifs = data->in_ifs_non_space = false;
				continue;
			}

			bool is_ifs_non_space = strchr(data->ifs_non_space, c) != NULL;
			if (!data->in_ifs || (is_ifs_non_space && data->in_ifs_non_space)) {
				mrsh_buffer_append_char(&buf, '\0');
				char *str = mrsh_buffer_steal(&buf);
				add_to_cur_field(data,
					&mrsh_word_string_create(str, false)->word);
				data->cur_field = NULL;
				data->in_ifs = true;
			} else if (is_ifs_non_space) {
				data->in_ifs_non_space = true;
			}
		}

		if (!data->in_ifs) {
			mrsh_buffer_append_char(&buf, '\0');
			char *str = mrsh_buffer_steal(&buf);
			add_to_cur_field(data,
				&mrsh_word_string_create(str, false)->word);
		}

		mrsh_buffer_finish(&buf);
		break;
	case MRSH_WORD_LIST:;
		const struct mrsh_word_list *wl = mrsh_word_get_list(word);

		if (wl->double_quoted) {
			add_to_cur_field(data, mrsh_word_copy(word));
			return;
		}

		for (size_t i = 0; i < wl->children.len; ++i) {
			const struct mrsh_word *child = wl->children.data[i];
			_split_fields(data, child);
		}
		break;
	default:
		assert(false);
	}
}

void split_fields(struct mrsh_array *fields, const struct mrsh_word *word,
		const char *ifs) {
	if (ifs == NULL) {
		ifs = " \t\n";
	} else if (ifs[0] == '\0') {
		mrsh_array_add(fields, mrsh_word_copy(word));
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

	struct split_fields_data data = {
		.fields = fields,
		.ifs = ifs,
		.ifs_non_space = ifs_non_space,
		.in_ifs = true,
	};
	_split_fields(&data, word);

	free(ifs_non_space);
}

void get_fields_str(struct mrsh_array *strs, const struct mrsh_array *fields) {
	for (size_t i = 0; i < fields->len; i++) {
		struct mrsh_word *word = fields->data[i];
		mrsh_array_add(strs, mrsh_word_str(word));
	}
}

static bool is_pathname_metachar(char c) {
	switch (c) {
	case '*':
	case '?':
	case '[':
	case ']':
		return true;
	default:
		return false;
	}
}

static bool needs_pathname_expansion(const struct mrsh_word *word) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		const struct mrsh_word_string *ws = mrsh_word_get_string(word);
		if (ws->single_quoted) {
			return false;
		}

		size_t len = strlen(ws->str);
		for (size_t i = 0; i < len; i++) {
			if (is_pathname_metachar(ws->str[i])) {
				return true;
			}
		}
		return false;
	case MRSH_WORD_LIST:;
		const struct mrsh_word_list *wl = mrsh_word_get_list(word);
		if (wl->double_quoted) {
			return false;
		}

		for (size_t i = 0; i < wl->children.len; i++) {
			const struct mrsh_word *child = wl->children.data[i];
			if (needs_pathname_expansion(child)) {
				return true;
			}
		}
		return false;
	default:
		assert(false);
	}

}

static void _word_to_pattern(struct mrsh_buffer *buf,
		const struct mrsh_word *word, bool quoted) {
	switch (word->type) {
	case MRSH_WORD_STRING:;
		const struct mrsh_word_string *ws = mrsh_word_get_string(word);

		size_t len = strlen(ws->str);
		for (size_t i = 0; i < len; i++) {
			char c = ws->str[i];
			if (is_pathname_metachar(c) && (quoted || ws->single_quoted)) {
				mrsh_buffer_append_char(buf, '\\');
			}
			mrsh_buffer_append_char(buf, c);
		}
		break;
	case MRSH_WORD_LIST:;
		const struct mrsh_word_list *wl = mrsh_word_get_list(word);

		for (size_t i = 0; i < wl->children.len; i++) {
			const struct mrsh_word *child = wl->children.data[i];
			_word_to_pattern(buf, child, quoted || wl->double_quoted);
		}
		break;
	default:
		assert(false);
	}
}

char *word_to_pattern(const struct mrsh_word *word) {
	if (!needs_pathname_expansion(word)) {
		return NULL;
	}

	struct mrsh_buffer buf = {0};
	_word_to_pattern(&buf, word, false);
	mrsh_buffer_append_char(&buf, '\0');
	return mrsh_buffer_steal(&buf);
}

bool expand_pathnames(struct mrsh_array *expanded,
		const struct mrsh_array *fields) {
	for (size_t i = 0; i < fields->len; ++i) {
		const struct mrsh_word *field = fields->data[i];

		char *pattern = word_to_pattern(field);
		if (pattern == NULL) {
			mrsh_array_add(expanded, mrsh_word_str(field));
			continue;
		}

		glob_t glob_buf;
		int ret = glob(pattern, GLOB_NOSORT, NULL, &glob_buf);
		if (ret == 0) {
			for (size_t i = 0; i < glob_buf.gl_pathc; ++i) {
				mrsh_array_add(expanded, strdup(glob_buf.gl_pathv[i]));
			}
			globfree(&glob_buf);
		} else if (ret == GLOB_NOMATCH) {
			mrsh_array_add(expanded, mrsh_word_str(field));
		} else {
			fprintf(stderr, "glob failed: %d\n", ret);
			free(pattern);
			return false;
		}

		free(pattern);
	}

	return true;
}
