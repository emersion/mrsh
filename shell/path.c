#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <mrsh/buffer.h>
#include <mrsh/shell.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "shell/path.h"

char *expand_path(struct mrsh_state *state, const char *file, bool exec,
		bool default_path) {
	if (strchr(file, '/')) {
		return strdup(file);
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

	char *path = NULL;
	char *basedir = strtok(pathe, ":");
	while (basedir != NULL) {
		size_t blen = strlen(basedir);
		if (blen == 0) {
			goto next;
		}
		bool slash = basedir[blen - 1] == '/';
		size_t n = snprintf(NULL, 0, "%s%s%s", basedir, slash ? "" : "/", file);
		path = realloc(path, n + 1);
		if (path == NULL) {
			goto next;
		}
		snprintf(path, n + 1, "%s%s%s", basedir, slash ? "" : "/", file);
		if (access(path, exec ? X_OK : R_OK) != -1) {
			free(pathe);
			return path;
		}
next:
		basedir = strtok(NULL, ":");
	}
	free(path);
	free(pathe);
	return NULL;
}

char *current_working_dir(void) {
	// POSIX doesn't provide a way to query the CWD size
	struct mrsh_buffer buf = {0};
	if (mrsh_buffer_reserve(&buf, 256) == NULL) {
		return NULL;
	}
	while (getcwd(buf.data, buf.cap) == NULL) {
		if (errno != ERANGE) {
			return NULL;
		}
		if (mrsh_buffer_reserve(&buf, buf.cap * 2) == NULL) {
			return NULL;
		}
	}
	return mrsh_buffer_steal(&buf);
}
