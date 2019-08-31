#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/job.h"
#include "shell/shell.h"

static const char fg_usage[] = "usage: fg [job_id]\n";

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

	struct mrsh_job *job;
	if (mrsh_optind == argc) {
		job = job_by_id(state, "%%", true);
	} else if (mrsh_optind == argc - 1) {
		job = job_by_id(state, argv[mrsh_optind], true);
	} else {
		fprintf(stderr, fg_usage);
		return EXIT_FAILURE;
	}
	if (!job) {
		return EXIT_FAILURE;
	}

	if (!job_set_foreground(job, true, true)) {
		return EXIT_FAILURE;
	}
	return job_wait(job);
}
