#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "shell/task.h"

static const char return_usage[] = "usage: %s [n]\n";

int builtin_return(struct mrsh_state *state, int argc, char *argv[]) {
	if (argc > 2) {
		fprintf(stderr, return_usage, argv[0]);
		return 1;
	}

	int n = 0;
	if (argc == 2) {
		char *end;
		n = strtol(argv[1], &end, 10);
		if (end[0] != '\0' || argv[0][0] == '\0' || n < 0 || n > 255) {
			fprintf(stderr, "%s: invalid return number '%s'\n", argv[0], argv[1]);
			return 1;
		}
	}

	struct mrsh_call_frame_priv *frame_priv = call_frame_get_priv(state->frame);

	frame_priv->nloops = 0;
	frame_priv->branch_control = MRSH_BRANCH_RETURN;
	state->last_status = n;
	return TASK_STATUS_INTERRUPTED;
}
