#include <assert.h>
#include <limits.h>
#include <mrsh/getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "builtin.h"
#include "shell/job.h"
#include "shell/process.h"
#include "shell/shell.h"
#include "shell/task.h"

static const char jobs_usage[] = "usage: jobs [-l|-p] [job_id...]\n";

static char *job_state_str(struct mrsh_job *job, bool r) {
	int status = job_poll(job);
	switch (status) {
	case TASK_STATUS_WAIT:
		return "Running";
	case TASK_STATUS_ERROR:
		return "Error";
	case TASK_STATUS_STOPPED:
		if (job->processes.len > 0) {
			struct process *proc = job->processes.data[0];
			switch (proc->signal) {
			case SIGSTOP:
				return r ? "Stopped (SIGSTOP)" : "Suspended (SIGSTOP)";
			case SIGTTIN:
				return r ? "Stopped (SIGTTIN)" : "Suspended (SIGTTIN)";
			case SIGTTOU:
				return r ? "Stopped (SIGTTOU)" : "Suspended (SIGTTOU)";
			}
		}
		return r ? "Stopped" : "Suspended";
	default:
		if (job->processes.len > 0) {
			struct process *proc = job->processes.data[0];
			if (proc->stat != 0) {
				static char stat[128];
				snprintf(stat, sizeof(stat), "Done(%d)", proc->stat);
				return stat;
			}
		}
		assert(status >= 0);
		return "Done";
	}
}

struct jobs_context {
	struct mrsh_job *current, *previous;
	bool pids;
	bool pgids;
	int r;
};

static void show_job(struct mrsh_job *job, const struct jobs_context *ctx) {
	if (job_poll(job) >= 0) {
		return;
	}
	char curprev = ' ';
	if (job == ctx->current) {
		curprev = '+';
	} else if (job == ctx->previous) {
		curprev = '-';
	}
	if (ctx->pids) {
		for (size_t i = 0; i < job->processes.len; ++i) {
			struct process *proc = job->processes.data[i];
			printf("%d\n", proc->pid);
		}
	} else if (ctx->pgids) {
		char *cmd = mrsh_node_format(job->node);
		printf("[%d] %c %d %s %s\n", job->job_id, curprev, job->pgid,
				job_state_str(job, ctx->r), cmd);
		free(cmd);
	} else {
		char *cmd = mrsh_node_format(job->node);
		printf("[%d] %c %s %s\n", job->job_id, curprev,
				job_state_str(job, ctx->r), cmd);
		free(cmd);
	}
}

int builtin_jobs(struct mrsh_state *state, int argc, char *argv[]) {
	struct mrsh_job *current = job_by_id(state, "%+", false),
		*previous = job_by_id(state, "%-", false);

	struct jobs_context ctx = {
		.current = current,
		.previous = previous,
		.pids = false,
		.pgids = false,
		.r = rand() % 2 == 0,
	};

	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":lp")) != -1) {
		switch (opt) {
		case 'l':
			if (ctx.pids) {
				fprintf(stderr, "jobs: the -p and -l options are "
						"mutually exclusive\n");
				return EXIT_FAILURE;
			}
			ctx.pgids = true;
			break;
		case 'p':
			if (ctx.pgids) {
				fprintf(stderr, "jobs: the -p and -l options are "
						"mutually exclusive\n");
				return EXIT_FAILURE;
			}
			ctx.pids = true;
			break;
		default:
			fprintf(stderr, "jobs: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, jobs_usage);
			return EXIT_FAILURE;
		}
	}

	if (mrsh_optind == argc) {
		for (size_t i = 0; i < state->jobs.len; i++) {
			struct mrsh_job *job = state->jobs.data[i];
			show_job(job, &ctx);
		}
	} else {
		for (int i = mrsh_optind; i < argc; i++) {
			struct mrsh_job *job = job_by_id(state, argv[i], true);
			if (!job) {
				return 1;
			}
			show_job(job, &ctx);
		}
	}

	return EXIT_SUCCESS;
}
