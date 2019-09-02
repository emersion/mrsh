#include <errno.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/task.h"

static const char exit_usage[] = "usage: exit [n]\n";

int builtin_exit(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, exit_usage);
		return 1;
	}

	int status = 0;
	if (argc > 1) {
		char *endptr;
		errno = 0;
		long status_long = strtol(argv[1], &endptr, 10);
		if (endptr[0] != '\0' || errno != 0 ||
				status_long < 0 || status_long > 255) {
			fprintf(stderr, exit_usage);
			return 1;
		}
		status = (int)status_long;
	}

	state->exit = status;
	state->frame->branch_control = MRSH_BRANCH_EXIT;
	return TASK_STATUS_INTERRUPTED;
}
