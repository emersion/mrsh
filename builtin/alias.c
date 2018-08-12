#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "builtin.h"

// usage: alias [alias-name[=string]...]

static void print_alias_iterator(const char *key, void *_value,
		void *user_data) {
	const char *value = _value;
	printf("%s=", key);
	print_escaped(value);
	printf("\n");
}

int builtin_alias(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc == 1) {
		mrsh_hashtable_for_each(&state->aliases, print_alias_iterator, NULL);
		return EXIT_SUCCESS;
	}

	for (int i = 1; i < argc; ++i) {
		char *alias = argv[i];
		char *equal = strchr(alias, '=');
		if (equal != NULL) {
			char *value = strdup(equal + 1);
			*equal = '\0';

			char *old_value = mrsh_hashtable_set(&state->aliases, alias, value);
			free(old_value);
		} else {
			const char *value = mrsh_hashtable_get(&state->aliases, alias);
			if (value == NULL) {
				fprintf(stderr, "%s: %s not found\n", argv[0], alias);
				return EXIT_FAILURE;
			}

			printf("%s=", alias);
			print_escaped(value);
			printf("\n");
		}
	}

	return EXIT_SUCCESS;
}
