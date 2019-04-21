#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/job.h"
#include "shell/shell.h"
#include "shell/task.h"

// TODO: bg [job_id]
static const char bg_usage[] = "usage: bg\n";

int builtin_bg(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":")) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "bg: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, bg_usage);
			return EXIT_FAILURE;
		}
	}
	if (mrsh_optind < argc) {
		fprintf(stderr, bg_usage);
		return EXIT_FAILURE;
	}

	struct mrsh_job *stopped = NULL;
	for (ssize_t i = state->jobs.len - 1; i >= 0; --i) {
		struct mrsh_job *job = state->jobs.data[i];
		if (job_poll(job) == TASK_STATUS_STOPPED) {
			stopped = job;
			break;
		}
	}
	if (stopped == NULL) {
		fprintf(stderr, "bg: no current job");
		return EXIT_FAILURE;
	}

	if (!job_set_foreground(stopped, false, true)) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
