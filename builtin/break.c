#define _POSIX_C_SOURCE 200809L
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "shell/shell.h"
#include "shell/task.h"

static const char break_usage[] = "usage: %s [n]\n";

int builtin_break(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, break_usage, argv[0]);
		return 1;
	}

	int n = 1;
	if (argc == 2) {
		char *end;
		n = strtol(argv[1], &end, 10);
		if (end[0] != '\0' || argv[0][0] == '\0' || n < 0) {
			fprintf(stderr, "%s: invalid loop number '%s'\n", argv[0], argv[1]);
			return 1;
		}
	}

	if (n > state->nloops) {
		n = state->nloops;
	}

	state->nloops -= n - 1;
	state->branch_control =
		strcmp(argv[0], "break") == 0 ? MRSH_BRANCH_BREAK : MRSH_BRANCH_CONTINUE;
	return TASK_STATUS_INTERRUPTED;
}
