#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char export_usage[] = "usage: export (-p|name[=word]...)\n";

int builtin_export(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, export_usage);
		return EXIT_FAILURE;
	} else if (argc == 2 && strcmp(argv[1], "-p") == 0) {
		size_t count;
		struct mrsh_collect_var *vars = mrsh_collect_vars(
				state, MRSH_VAR_ATTRIB_EXPORT, &count);
		for (size_t i = 0; i < count; ++i) {
			printf("export %s=", vars[i].key);
			print_escaped(vars[i].value);
			printf("\n");
		}
		free(vars);
		return EXIT_SUCCESS;
	}

	for (int i = 1; i < argc; ++i) {
		char *eql, *key;
		const char *val;
		eql = strchr(argv[i], '=');
		if (eql) {
			size_t klen = eql - argv[i];
			key = strndup(argv[i], klen);
			val = &eql[1];
		} else {
			key = strdup(argv[i]);
			val = mrsh_env_get(state, key, NULL);
			if (!val) {
				val = "";
			}
		}
		mrsh_env_set(state, key, val, MRSH_VAR_ATTRIB_EXPORT);
		free(key);
	}

	return EXIT_SUCCESS;
}
