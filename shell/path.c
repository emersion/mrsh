#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <mrsh/shell.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "shell/path.h"

const char *expand_path(struct mrsh_state *state, const char *file, bool exec) {
	if (strchr(file, '/')) {
		return file;
	}
	static char path[PATH_MAX + 1];
	const char *_pathe = mrsh_env_get(state, "PATH", NULL);
	if (!_pathe) {
		return NULL;
	}
	char *pathe = strdup(_pathe);
	if (!pathe) {
		return NULL;
	}
	char *basedir = strtok(pathe, ":");
	while (basedir) {
		int blen = strlen(basedir);
		if (blen == 0) {
			goto next;
		}
		bool slash = basedir[blen - 1] == '/';
		size_t n = snprintf(path, sizeof(path), "%s%s%s",
				basedir, slash ? "" : "/", file);
		if (n >= sizeof(path)) {
			goto next;
		}
		if (access(path, exec ? X_OK : R_OK) != -1) {
			free(pathe);
			return path;
		}
next:
		basedir = strtok(NULL, ":");
	}
	free(pathe);
	return NULL;
}

static struct mrsh_cached_command_path *cache_exec_path(struct mrsh_state *state, 
		const char *command, const char *path) {
	const char* key = strdup(command);

	struct mrsh_hashtable *paths = &state->cached_command_paths;
	struct mrsh_cached_command_path *entry = 
		calloc(1, sizeof(struct mrsh_cached_command_path));

	entry->command = strdup(path);
	mrsh_hashtable_set(paths, key, entry);

	return entry;
}

const char *expand_exec_path(struct mrsh_state *state, const char *command) {
	if (strlen(command) == 0) {
		return NULL;
	}

	struct mrsh_cached_command_path *entry;

	if (command[0] != '/') {
		struct mrsh_hashtable *paths = &state->cached_command_paths;
		entry = (struct mrsh_cached_command_path *)mrsh_hashtable_get(paths, command);

		if (entry != NULL) {
			entry->hits ++;
			return entry->command;
		}
	}

	const char* path = expand_path(state, command, true);

	if (path == NULL) {
		return NULL;
	}

	entry = cache_exec_path(state, command, path);
	entry->hits ++;

	return entry->command;
}

static void free_entry(const char *key, void *value, void *user_data) {
	struct mrsh_cached_command_path *entry = value;
	free(entry->command);
	mrsh_hashtable_del((struct mrsh_hashtable *)user_data, key);
}

void clear_exec_path_cache(struct mrsh_state *state) {
	mrsh_hashtable_for_each(&state->cached_command_paths, free_entry,
		&state->cached_command_paths);
}
