#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/job.h"
#include "shell/shell.h"

// TODO: fg [job_id]
static const char fg_usage[] = "usage: fg\n";

int builtin_fg(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":")) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "fg: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, fg_usage);
			return EXIT_FAILURE;
		}
	}
	if (mrsh_optind < argc) {
		fprintf(stderr, fg_usage);
		return EXIT_FAILURE;
	}

	struct mrsh_job *stopped = NULL;
	for (ssize_t i = state->jobs.len - 1; i >= 0; --i) {
		struct mrsh_job *job = state->jobs.data[i];
		if (job_stopped(job)) {
			stopped = job;
			break;
		}
	}
	if (stopped == NULL) {
		fprintf(stderr, "fg: no current job");
		return EXIT_FAILURE;
	}

	if (!job_set_foreground(stopped, true, true)) {
		return EXIT_FAILURE;
	}
	return job_wait(stopped);
}
