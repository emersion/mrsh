#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "mrsh_limit.h"
#include "shell/path.h"

static const char cd_usage[] = "usage: cd [-L|-P] [-|directory]\n";

static int cd(struct mrsh_state *state, const char *path) {
	const char *oldPWD = mrsh_env_get(state, "PWD", NULL);
	if (chdir(path) != 0) {
		// TODO make better error messages
		fprintf(stderr, "cd: %s\n", strerror(errno));
		return 1;
	}
	char *cwd = current_working_dir();
	if (cwd == NULL) {
		perror("current_working_dir failed");
		return 1;
	}
	mrsh_env_set(state, "OLDPWD", oldPWD, MRSH_VAR_ATTRIB_NONE);
	mrsh_env_set(state, "PWD", cwd, MRSH_VAR_ATTRIB_EXPORT);
	free(cwd);
	return 0;
}

static int isdir(char *path) {
	struct stat s;
	stat(path, &s);
	return S_ISDIR(s.st_mode);
}

int builtin_cd(struct mrsh_state *state, int argc, char *argv[]) {
	_mrsh_optind = 0;
	int opt;
	while ((opt = _mrsh_getopt(argc, argv, ":LP")) != -1) {
		switch (opt) {
		case 'L':
		case 'P':
			// TODO implement `-L` and `-P`
			fprintf(stderr, "cd: `-L` and `-P` not yet implemented\n");
			return 1;
		default:
			fprintf(stderr, "cd: unknown option -- %c\n", _mrsh_optopt);
			fprintf(stderr, cd_usage);
			return 1;
		}
	}

	if (_mrsh_optind == argc) {
		const char *home = mrsh_env_get(state, "HOME", NULL);
		if (home && home[0] != '\0') {
			return cd(state, home);
		}
		fprintf(stderr, "cd: No arguments were given and $HOME "
			"is not defined.\n");
		return 1;
	}

	char *curpath = argv[_mrsh_optind];
	// `cd -`
	if (strcmp(curpath, "-") == 0) {
		// This case is special as we print `pwd` at the end
		const char *oldpwd = mrsh_env_get(state, "OLDPWD", NULL);
		const char *pwd = mrsh_env_get(state, "PWD", NULL);
		if (!pwd) {
			fprintf(stderr, "cd: PWD is not set\n");
			return 1;
		}
		if (!oldpwd) {
			fprintf(stderr, "cd: OLDPWD is not set\n");
			return 1;
		}
		if (chdir(oldpwd) != 0) {
			fprintf(stderr, "cd: %s\n", strerror(errno));
			return 1;
		}
		char *_pwd = strdup(pwd);
		puts(oldpwd);
		mrsh_env_set(state, "PWD", oldpwd, MRSH_VAR_ATTRIB_EXPORT);
		mrsh_env_set(state, "OLDPWD", _pwd, MRSH_VAR_ATTRIB_NONE);
		free(_pwd);
		return 0;
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
				len = snprintf(path, PATH_MAX, "%s/%s", c, curpath);
			} else {
				len = snprintf(path, PATH_MAX, "%s%s", c, curpath);
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
