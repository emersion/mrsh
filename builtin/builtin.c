#include <assert.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "builtin.h"

struct builtin {
	const char *name;
	mrsh_builtin_func_t func;
	bool special;
};

static const struct builtin builtins[] = {
	// Keep alpha sorted
	{ ":", builtin_colon, true },
	{ "alias", builtin_alias, false },
	{ "cd", builtin_cd, true },
	{ "exit", builtin_exit, true },
	{ "set", builtin_set, true },
	{ "times", builtin_times, true },
};

static int builtin_compare(const void *_a, const void *_b) {
	const struct builtin *a = _a, *b = _b;
	return strcmp(a->name, b->name);
}

static const struct builtin *get_builtin(const char *name) {
	struct builtin key = { .name = name };
	return bsearch(&key, builtins, sizeof(builtins) / sizeof(builtins[0]),
		sizeof(builtins[0]), builtin_compare);
}

bool mrsh_has_builtin(const char *name) {
	return get_builtin(name) != NULL;
}

bool mrsh_has_special_builtin(const char *name) {
	const struct builtin *builtin = get_builtin(name);
	return builtin != NULL && builtin->special;
}

int mrsh_run_builtin(struct mrsh_state *state, int argc, char *argv[]) {
	assert(argc > 0);

	const char *name = argv[0];
	const struct builtin *builtin = get_builtin(name);
	if (builtin == NULL) {
		return -1;
	}

	return builtin->func(state, argc, argv);
}

void print_escaped(const char *value) {
	const char safe[] = "@%+=:,./-";
	size_t i;
	for (i = 0; value[i] != '\0'; ++i) {
		if (!isalnum(value[i]) && !strchr(safe, value[i])) {
			break;
		}
	}
	if (value[i] == '\0') {
		printf("%s", value);
	} else {
		printf("'");
		for (size_t i = 0; value[i] != '\0'; ++i) {
			if (value[i] == '\'') {
				printf("'\"'\"'");
			} else {
				printf("%c", value[i]);
			}
		}
		printf("'");
	}
}
