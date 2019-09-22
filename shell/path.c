#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <mrsh/shell.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "shell/path.h"

const char *expand_path(struct mrsh_state *state, const char *file, bool exec,
		bool default_path) {
	if (strchr(file, '/')) {
		return file;
	}

	char *pathe;
	if (!default_path) {
		const char *_pathe = mrsh_env_get(state, "PATH", NULL);
		if (!_pathe) {
			return NULL;
		}
		pathe = strdup(_pathe);
		if (!pathe) {
			return NULL;
		}
	} else {
		size_t pathe_size = confstr(_CS_PATH, NULL, 0);
		if (pathe_size == 0) {
			return NULL;
		}
		pathe = malloc(pathe_size);
		if (pathe == NULL) {
			return NULL;
		}
		if (confstr(_CS_PATH, pathe, pathe_size) != pathe_size) {
			free(pathe);
			return NULL;
		}
	}

	static char path[PATH_MAX + 1];
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
