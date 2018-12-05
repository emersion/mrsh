#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/shell.h>
#include <shell/word.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"

static const char read_usage[] = "usage: read [-r] var...\n";

int builtin_read(struct mrsh_state *state, int argc, char *argv[]) {
	bool raw = false;

	optind = 0;
	int opt;
	while ((opt = getopt(argc, argv, ":r")) != -1) {
		switch (opt) {
		case 'r':
			raw = true;
			break;
		default:
			fprintf(stderr, "read: unknown option -- %c\n", optopt);
			fprintf(stderr, read_usage);
			return EXIT_FAILURE;
		}
	}
	if (optind == argc) {
		fprintf(stderr, read_usage);
		return EXIT_FAILURE;
	}

	struct mrsh_buffer buf = {0};
	bool escaped = false;
	int c;
	while ((c = fgetc(stdin)) != EOF) {
		if (!raw && !escaped && c == '\\') {
			escaped = true;
			continue;
		}
		if (c == '\n') {
			if (escaped) {
				escaped = false;
				const char *ps2 = mrsh_env_get(state, "PS2", NULL);
				fprintf(stderr, "%s", ps2 != NULL ? ps2 : "> ");
				continue;
			}
			break;
		}
		escaped = false;
		mrsh_buffer_append_char(&buf, (char)c);
	}
	mrsh_buffer_append_char(&buf, '\0');

	struct mrsh_array fields = {0};

	struct mrsh_word_string *ws = mrsh_word_string_create(mrsh_buffer_steal(&buf), false);
	split_fields(&fields, &ws->word, mrsh_env_get(state, "IFS", NULL));
	mrsh_word_destroy(&ws->word);

	if (fields.len <= (size_t)(argc - optind)) {
		for (size_t i = 0; i < fields.len; ++i) {
			mrsh_env_set(state, argv[optind + i], (char *)fields.data[i], MRSH_VAR_ATTRIB_NONE);
		}
		for (size_t i = fields.len; i < (size_t)(argc - optind); ++i) {
			mrsh_env_set(state, argv[optind + i], "", MRSH_VAR_ATTRIB_NONE);
		}
	} else {
		for (int i = 0; i < argc - optind - 1; ++i) {
			mrsh_env_set(state, argv[optind + i], (char *)fields.data[i], MRSH_VAR_ATTRIB_NONE);
		}
		struct mrsh_buffer buf_last = {0};
		for (size_t i = (size_t)(argc - optind - 1); i < fields.len; ++i) {
			char *field = (char *)fields.data[i];
			mrsh_buffer_append(&buf_last, field, strlen(field));
			if (i != fields.len - 1) {
				// TODO add the field delimiter rather than space (bash and dash always use spaces)
				mrsh_buffer_append_char(&buf_last, ' ');
			}
		}
		mrsh_buffer_append_char(&buf_last, '\0');
		mrsh_env_set(state, argv[argc - 1], buf_last.data, MRSH_VAR_ATTRIB_NONE);
		mrsh_buffer_finish(&buf_last);
	}

	for (size_t i = 0; i < fields.len; ++i) {
		free(fields.data[i]);
	}
	mrsh_array_finish(&fields);
	return EXIT_SUCCESS;
}
