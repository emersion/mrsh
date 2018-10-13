#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mrsh/shell.h>
#include "builtin.h"

// usage: cd [-|directory]

static int cd(struct mrsh_state *state, const char *path) {
	const char *oldPWD = mrsh_env_get(state, "PWD", NULL);
	if (chdir(path) != 0) {
		// TODO make better error messages
		fprintf(stderr, "cd: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	char cwd[PATH_MAX];
	if (getcwd(cwd, PATH_MAX) == NULL) {
		fprintf(stderr, "cd: Cannot set new PWD as the path "
			"is too long\n");
		return EXIT_FAILURE;
	}
	mrsh_env_set(state, "OLDPWD", oldPWD, MRSH_VAR_ATTRIB_NONE);
	mrsh_env_set(state, "PWD", cwd, MRSH_VAR_ATTRIB_EXPORT);
	return EXIT_SUCCESS;
}

static int isdir(char *path) {
	struct stat s;
	stat(path, &s);
	return S_ISDIR(s.st_mode);
}

int builtin_cd(struct mrsh_state *state, int argc, char *argv[]) {
	// TODO `-P` and `-L`
	if (argc > 2) {
		fprintf(stderr, "Too many args for cd\n");
		return EXIT_FAILURE;
	} else if (argc == 1) {
		const char *home = mrsh_env_get(state, "HOME", NULL);
		if (home && home[0] != '\0') {
			return cd(state, home);
		}
		fprintf(stderr, "cd: No arguments were given and $HOME "
			"is not defined.\n");
		return EXIT_FAILURE;
	}
	char *curpath = argv[1];
	// `cd -`
	if (strcmp(curpath, "-") == 0) {
		// This case is special as we print `pwd` at the end
		const char *oldpwd = mrsh_env_get(state, "OLDPWD", NULL);
		const char *pwd = mrsh_env_get(state, "PWD", NULL);
		if (!pwd) {
			fprintf(stderr, "cd: PWD is not set\n");
			return EXIT_FAILURE;
		}
		if (!oldpwd) {
			fprintf(stderr, "cd: OLDPWD is not set\n");
			return EXIT_FAILURE;
		}
		if (chdir(oldpwd) != 0) {
			fprintf(stderr, "cd: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		char *_pwd = strdup(pwd);
		puts(oldpwd);
		mrsh_env_set(state, "PWD", oldpwd, MRSH_VAR_ATTRIB_EXPORT);
		mrsh_env_set(state, "OLDPWD", _pwd, MRSH_VAR_ATTRIB_NONE);
		free(_pwd);
		return EXIT_SUCCESS;
	}
	// $CDPATH
	if (curpath[0] != '/' && strncmp(curpath, "./", 2) != 0 &&
			strncmp(curpath, "../", 3) != 0) {
		const char *_cdpath = mrsh_env_get(state, "CDPATH", NULL);
		char *cdpath = NULL;
		if (_cdpath) {
			cdpath = strdup(_cdpath);
		}
		char *c = cdpath;
		while (c != NULL) {
			char *next = strchr(c, ':');
			char *slash = strrchr(c, '/');
			if (next) {
				*next = '\0';
				++next;
			}
			if (*c == '\0') {
				// path is empty
				c = ".";
				slash = NULL;
			}
			int len;
			char path[PATH_MAX];
			if (slash == NULL || slash[1] != '\0') {
				// the last character is not a slash
				len = snprintf(path, PATH_MAX, "%s/%s", c,
					curpath);
			} else {
				len = snprintf(path, PATH_MAX, "%s%s", c,
					curpath);
			}
			if (len >= PATH_MAX) {
				fprintf(stderr, "cd: Cannot search $CDPATH "
					"directory \"%s\" since it exceeds the "
					"maximum path length\n", c);
				continue;
			}
			if (isdir(path)) {
				free(cdpath);
				return cd(state, path);
			}
			c = next;
		}
		free(cdpath);
	}
	return cd(state, curpath);
}
