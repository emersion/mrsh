#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "builtin.h"
#include "mrsh_getopt.h"

static const char umask_usage[] = "usage: umask [-S] [mode]\n";

enum umask_symbolic_state {
	UMASK_WHO,
	UMASK_PERM,
};

static mode_t umask_current_mask(void) {
	const mode_t default_mode = 0022;

	mode_t mask = umask(default_mode);
	umask(mask);
	return mask;
}

static bool umask_update_mode(mode_t *mode, char action, mode_t *perm_mask, mode_t *who_mask) {
	switch (action) {
	case '+':
		*mode |= (*who_mask & *perm_mask);
		break;
	case '=':
		*mode = (*mode & ~(*who_mask)) | (*perm_mask & *who_mask);
		break;
	case '-':
		*mode &= (0777 & ~(*who_mask & *perm_mask));
		break;
	default:
		fprintf(stderr, "unknown action -- '%c'\n", action);
		return false;
	}

	*perm_mask = *who_mask = 0;
	return true;
}

static bool umask_mode(mode_t *mode, char *symbolic) {
	mode_t tmp_mode = 0777 & ~(umask_current_mask());
	enum umask_symbolic_state state = UMASK_WHO;
	mode_t who_mask = 0;
	mode_t perm_mask = 0;
	char action = '\0';

	for (char *c = symbolic; *c != '\0'; c++) {
		switch (state) {
		case UMASK_WHO:
			switch (*c) {
			case 'u':
				who_mask |= S_IRWXU;
				break;
			case 'g':
				who_mask |= S_IRWXG;
				break;
			case 'o':
				who_mask |= S_IRWXO;
				break;
			case 'a':
				who_mask |= (S_IRWXU | S_IRWXG | S_IRWXO);
				break;
			case '+':
			case '-':
			case '=':
				if (who_mask == 0) {
					who_mask |= (S_IRWXU | S_IRWXG | S_IRWXO);
				}
				action = *c;
				state = UMASK_PERM;
				break;
			default:
				fprintf(stderr, "Unknown who -- '%c'\n", *c);
				return false;
			}

			break;

		case UMASK_PERM:
			switch (*c) {
			case 'u':
				perm_mask = (tmp_mode & S_IRWXU) | ((tmp_mode & S_IRWXU) >> 3) | ((tmp_mode & S_IRWXU) >> 6);
				break;
			case 'g':
				perm_mask = ((tmp_mode & S_IRWXG) << 3) | (tmp_mode & S_IRWXG) | ((tmp_mode & S_IRWXG) >> 3);
				break;
			case 'o':
				perm_mask = ((tmp_mode & S_IRWXO) << 6) | ((tmp_mode & S_IRWXO) << 3) | (tmp_mode & S_IRWXO);
				break;
			case 'r':
				perm_mask |= (S_IRUSR | S_IRGRP | S_IROTH);
				break;
			case 'w':
				perm_mask |= (S_IWUSR | S_IWGRP | S_IWOTH);
				break;
			case 'x':
				perm_mask |= (S_IXUSR | S_IXGRP | S_IXOTH);
				break;
			case ',':
				state = UMASK_WHO;
				if (!umask_update_mode(&tmp_mode, action, &perm_mask, &who_mask)) {
					return false;
				}
				break;
			default:
				fprintf(stderr, "Invalid permission -- '%c'\n", *c);
				return false;
			}
			break;
		}
	}

	if (state == UMASK_PERM) {
		if (!umask_update_mode(&tmp_mode, action, &perm_mask, &who_mask)) {
			return false;
		}
	} else {
		fprintf(stderr, "Missing permission from symbolic mode\n");
		return false;
	}

	*mode = 0777 & ~tmp_mode;

	return true;
}

static void umask_modestring(char string[static 4], mode_t mode) {
	size_t i = 0;

	if (S_IROTH & mode) {
		string[i++] = 'r';
	}

	if (S_IWOTH & mode) {
		string[i++] = 'w';
	}

	if (S_IXOTH & mode) {
		string[i++] = 'x';
	}
}

static void umask_print_symbolic(mode_t mask) {
	char user[4] = {0};
	char group[4] = {0};
	char other[4] = {0};
	mode_t mode = 0777 & ~mask;

	umask_modestring(user, (mode & 0700) >> 6);
	umask_modestring(group, (mode & 0070) >> 3);
	umask_modestring(other, (mode & 0007));

	printf("u=%s,g=%s,o=%s\n", user, group, other);
}

int builtin_umask(struct mrsh_state *state, int argc, char *argv[]) {
	mode_t mode;
	bool umask_symbolic = false;

	_mrsh_optind = 0;
	int opt;

	while ((opt = _mrsh_getopt(argc, argv, ":S")) != -1) {
		switch (opt) {
		case 'S':
			umask_symbolic = true;
			break;

		default:
			fprintf(stderr, "Unknown option -- '%c'\n", _mrsh_optopt);
			fprintf(stderr, umask_usage);
			return 1;
		}
	}

	if (_mrsh_optind == argc) {
		mode = umask_current_mask();

		if (umask_symbolic) {
			umask_print_symbolic(mode);
		} else {
			printf("%04o\n", mode);
		}

		return 0;
	}

	char *endptr;
	mode = strtol(argv[_mrsh_optind], &endptr, 8);

	if (*endptr != '\0') {
		if (!umask_mode(&mode, argv[_mrsh_optind])) {
			fprintf(stderr, umask_usage);
			return 1;
		}
	}

	umask(mode);
	return 0;
}
