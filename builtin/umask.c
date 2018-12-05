#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "builtin.h"

/*
 * TODO: Implement symbolic mode
 */

static const char umask_usage[] = "usage: umask [-S] [mode]\n";

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
	mode_t default_mode = 0022;
	bool umask_symbolic = false;

	optind = 0;
	int opt;
	while ((opt = getopt(argc, argv, ":S")) != -1) {
		switch(opt) {
		case 'S':
			umask_symbolic = true;
			break;
		default:
			fprintf(stderr, "Unknown option -- '%c'\n", optopt);
			fprintf(stderr, umask_usage);
			return EXIT_FAILURE;
		}
	}

	if (optind == argc) {
		mode = umask(default_mode);
		umask(mode);
		if (umask_symbolic) {
			umask_print_symbolic(mode);
		} else {
			printf("%04o\n", mode);
		}
		return EXIT_SUCCESS;
	}

	char *endptr;
	mode = strtol(argv[optind], &endptr, 8);
	if (*endptr != '\0') {
		fprintf(stderr, "Invalid mode %s\n", argv[optind]);
		fprintf(stderr, umask_usage);
		return EXIT_FAILURE;
	}

	umask(mode);
	return EXIT_SUCCESS;
}
