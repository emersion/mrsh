#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/job.h"
#include "shell/shell.h"
#include "shell/task.h"

static const char bg_usage[] = "usage: bg [job_id...]\n";

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
	if (mrsh_optind == argc) {
		struct mrsh_job *job = job_by_id(state, "%%");
		if (!job) {
			return EXIT_FAILURE;
		}
		if (!job_set_foreground(job, false, true)) {
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	for (int i = mrsh_optind; i < argc; ++i) {
		struct mrsh_job *job = job_by_id(state, argv[i]);
		if (!job) {
			return EXIT_FAILURE;
		}
		if (!job_set_foreground(job, false, true)) {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
