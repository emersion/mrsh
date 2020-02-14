#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "shell/shell.h"

static const char alias_usage[] = "usage: alias [alias-name[=string]...]\n";

static void print_alias_iterator(const char *key, void *_value,
		void *user_data) {
	const char *value = _value;
	printf("%s=", key);
	print_escaped(value);
	printf("\n");
}

int builtin_alias(struct mrsh_state *state, int argc, char *argv[]) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	_mrsh_optind = 0;
	if (_mrsh_getopt(argc, argv, ":") != -1) {
		fprintf(stderr, "alias: unknown option -- %c\n", _mrsh_optopt);
		fprintf(stderr, "%s", alias_usage);
		return 1;
	}

	if (_mrsh_optind == argc) {
		mrsh_hashtable_for_each(&priv->aliases, print_alias_iterator, NULL);
		return 0;
	}

	for (int i = _mrsh_optind; i < argc; ++i) {
		char *alias = argv[i];
		char *equal = strchr(alias, '=');
		if (equal != NULL) {
			char *value = strdup(equal + 1);
			*equal = '\0';

			char *old_value = mrsh_hashtable_set(&priv->aliases, alias, value);
			free(old_value);
		} else {
			const char *value = mrsh_hashtable_get(&priv->aliases, alias);
			if (value == NULL) {
				fprintf(stderr, "%s: %s not found\n", argv[0], alias);
				return 1;
			}

			printf("%s=", alias);
			print_escaped(value);
			printf("\n");
		}
	}

	return 0;
}
