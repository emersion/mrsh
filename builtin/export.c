#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char export_usage[] = "usage: %s -p|name[=word]...\n";

int builtin_export(struct mrsh_state *state, int argc, char *argv[]) {
	uint32_t attrib = MRSH_VAR_ATTRIB_EXPORT;
	if (strcmp(argv[0], "readonly") == 0) {
		attrib = MRSH_VAR_ATTRIB_READONLY;
	}

	if (argc < 2) {
		fprintf(stderr, export_usage, argv[0]);
		return EXIT_FAILURE;
	} else if (argc == 2 && strcmp(argv[1], "-p") == 0) {
		size_t count;
		struct mrsh_collect_var *vars = mrsh_collect_vars(
				state, MRSH_VAR_ATTRIB_EXPORT, &count);
		for (size_t i = 0; i < count; ++i) {
			printf("%s %s=", argv[0], vars[i].key);
			print_escaped(vars[i].value);
			printf("\n");
		}
		free(vars);
		return EXIT_SUCCESS;
	}

	for (int i = 1; i < argc; ++i) {
		char *eql, *key;
		const char *val;
		uint32_t prev_attribs = 0;
		eql = strchr(argv[i], '=');
		if (eql) {
			size_t klen = eql - argv[i];
			key = strndup(argv[i], klen);
			val = &eql[1];
			mrsh_env_get(state, key, &prev_attribs);
		} else {
			key = strdup(argv[i]);
			val = mrsh_env_get(state, key, &prev_attribs);
			if (!val) {
				val = "";
			}
		}
		if ((prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
			fprintf(stderr, "%s: cannot modify readonly variable %s\n",
					argv[0], key);
			free(key);
			return EXIT_FAILURE;
		}
		mrsh_env_set(state, key, val, attrib | prev_attribs);
		free(key);
	}

	return EXIT_SUCCESS;
}
