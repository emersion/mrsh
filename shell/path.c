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
	char *pathe = mrsh_hashtable_get(&state->variables, "PATH");
	if (!pathe || !(pathe = strdup(pathe))) {
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
