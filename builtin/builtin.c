#include <assert.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "builtin.h"

struct builtin {
	const char *name;
	mrsh_builtin_func func;
	bool special;
};

static const struct builtin builtins[] = {
	// Keep alpha sorted
	{ ".", builtin_dot, true },
	{ ":", builtin_colon, true },
	{ "alias", builtin_alias, false },
	{ "bg", builtin_bg, false },
	{ "cd", builtin_cd, false },
	{ "command", builtin_command, false },
	{ "eval", builtin_eval, true },
	{ "exit", builtin_exit, true },
	{ "export", builtin_export, true },
	{ "false", builtin_false, false },
	{ "fg", builtin_fg, false },
	{ "getopts", builtin_getopts, false },
	{ "pwd", builtin_pwd, false },
	{ "read", builtin_read, false },
	{ "readonly", builtin_export, true },
	{ "set", builtin_set, true },
	{ "shift", builtin_shift, true },
	{ "times", builtin_times, true },
	{ "true", builtin_true, false },
	{ "type", builtin_type, false },
	{ "ulimit", builtin_ulimit, false },
	{ "umask", builtin_umask, false },
	{ "unalias", builtin_unalias, false },
	{ "unset", builtin_unset, true },
	{ "wait", builtin_wait, false },
};

// The following commands are explicitly unspecified by POSIX
static const char *unspecified_names[] = {
	"alloc", "autoload", "bind", "bindkey", "builtin", "bye", "caller", "cap",
	"chdir", "clone", "comparguments", "compcall", "compctl", "compdescribe",
	"compfiles", "compgen", "compgroups", "complete", "compquote", "comptags",
	"comptry", "compvalues", "declare", "dirs", "disable", "disown", "dosh",
	"echotc", "echoti", "help", "history", "hist", "let", "local", "login",
	"logout", "map", "mapfile", "popd", "print", "pushd", "readarray", "repeat",
	"savehistory", "source", "shopt", "stop", "suspend", "typeset", "whence"
};

static const struct builtin unspecified = {
	.name = "unspecified",
	.func = builtin_unspecified,
	.special = false,
};

static int builtin_compare(const void *_a, const void *_b) {
	const struct builtin *a = _a, *b = _b;
	return strcmp(a->name, b->name);
}

static int unspecified_compare(const void *_a, const void *_b) {
	const char *a = _a;
	const char * const *b = _b;
	return strcmp(a, *b);
}

static const struct builtin *get_builtin(const char *name) {
	if (bsearch(name, unspecified_names,
			sizeof(unspecified_names) / sizeof(unspecified_names[0]),
			sizeof(unspecified_names[0]), unspecified_compare)) {
		return &unspecified;
	}
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

struct collect_iter {
	size_t len;
	size_t count;
	uint32_t attribs;
	struct mrsh_collect_var *values;
};

static void collect_vars(const char *key, void *_var, void *user_data) {
	const struct mrsh_variable *var = _var;
	struct collect_iter *iter = user_data;
	if (iter->attribs != MRSH_VAR_ATTRIB_NONE
			&& !(var->attribs & iter->attribs)) {
		return;
	}
	if ((iter->count + 1) * sizeof(struct mrsh_collect_var) >= iter->len) {
		iter->len *= 2;
		iter->values = realloc(iter->values,
				iter->len * sizeof(struct mrsh_collect_var));
	}
	iter->values[iter->count].key = key;
	iter->values[iter->count++].value = var->value;
}

static int varcmp(const void *p1, const void *p2) {
	const struct mrsh_collect_var *v1 = p1;
	const struct mrsh_collect_var *v2 = p2;
	return strcmp(v1->key, v2->key);
}

struct mrsh_collect_var *mrsh_collect_vars(struct mrsh_state *state,
		uint32_t attribs, size_t *count) {
	struct collect_iter iter = {
		.len = 64,
		.count = 0,
		.values = malloc(64 * sizeof(struct mrsh_collect_var)),
		.attribs = attribs,
	};
	mrsh_hashtable_for_each(&state->variables, collect_vars, &iter);
	qsort(iter.values, iter.count, sizeof(struct mrsh_collect_var), varcmp);
	*count = iter.count;
	return iter.values;
}
