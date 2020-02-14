#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "mrsh_getopt.h"
#include "shell/job.h"
#include "shell/shell.h"
#include "shell/task.h"

static const char bg_usage[] = "usage: bg [job_id...]\n";

int builtin_bg(struct mrsh_state *state, int argc, char *argv[]) {
	_mrsh_optind = 0;
	int opt;
	while ((opt = _mrsh_getopt(argc, argv, ":")) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "bg: unknown option -- %c\n", _mrsh_optopt);
			fprintf(stderr, bg_usage);
			return EXIT_FAILURE;
		}
	}
	if (_mrsh_optind == argc) {
		struct mrsh_job *job = job_by_id(state, "%%", true);
		if (!job) {
			return EXIT_FAILURE;
		}
		if (!job_set_foreground(job, false, true)) {
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	for (int i = _mrsh_optind; i < argc; ++i) {
		struct mrsh_job *job = job_by_id(state, argv[i], true);
		if (!job) {
			return EXIT_FAILURE;
		}
		if (!job_set_foreground(job, false, true)) {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
