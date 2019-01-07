#include <mrsh/array.h>
#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "builtin.h"
#include "shell/job.h"

// TODO: fg [job_id]
static const char fg_usage[] = "usage: fg\n";

int builtin_fg(struct mrsh_state *state, int argc, char *argv[]) {
	mrsh_optind = 1;
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

	struct job *bg = NULL;
	for (ssize_t i = jobs.len - 1; i >= 0; --i) {
		struct job *j = jobs.data[i];
		if (j != job_foreground()) {
			bg = j;
			break;
		}
	}
	if (bg == NULL) {
		fprintf(stderr, "fg: no current job");
		return EXIT_FAILURE;
	}

	job_set_foreground(bg, true);
	job_continue(bg);

	return EXIT_SUCCESS;
}
