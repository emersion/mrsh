#define _POSIX_C_SOURCE 200809L
#include <mrsh/getopt.h>
#include <mrsh/shell.h>
#include <shell/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"

static const char hash_usage[] = "usage: hash -r|utility...\n";

static void print_entry(const char *key, void *value, void *user_data)
{
	struct mrsh_cached_command_path *entry = value;
	unsigned int *count_entries = (unsigned int *)user_data;

	if (*count_entries == 0) {
		puts("hits\tcommand");
	}
	printf("%4d\t%s\n", entry->hits, entry->command);

	(*count_entries) ++;
}


static void print_table(struct mrsh_state *state) {
	unsigned int entries = 0;
	mrsh_hashtable_for_each(&state->cached_command_paths, print_entry, &entries);
}

static void add_command_path(struct mrsh_state *state, const char *command) {
	const char *path = expand_exec_path(state, command);

	if (path == NULL) {
		fprintf(stderr, "hash: command not found: %s\n", command);
		return;
	}

	struct mrsh_hashtable *paths = &state->cached_command_paths;
	struct mrsh_cached_command_path *entry =
		(struct mrsh_cached_command_path *)mrsh_hashtable_get(paths, command);

	entry->hits --;
}

int builtin_hash(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":r")) != -1) {
		switch (opt) {
		case 'r':
			clear_exec_path_cache(state);
			return 0;
		default:
			fprintf(stderr, "hash: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, hash_usage);
			return 1;
		}
	}

	if (argc == 1) {
		print_table(state);
		mrsh_optind ++;
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		add_command_path(state, argv[i]);
		mrsh_optind ++;
	}

	return 0;
}
