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

static const char jobs_usage[] = "usage: jobs\n";

static char *job_state_str(struct mrsh_job *job) {
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
				return "Stopped (SIGSTOP)";
			case SIGTTIN:
				return "Stopped (SIGTTIN)";
			case SIGTTOU:
				return "Stopped (SIGTTOU)";
			}
		}
		return "Stopped";
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

int builtin_jobs(struct mrsh_state *state, int argc, char *argv[]) {
	bool pids = false, pgids = false;

	mrsh_optind = 0;
	int opt;
	while ((opt = mrsh_getopt(argc, argv, ":lp")) != -1) {
		switch (opt) {
		case 'l':
			if (pids) {
				fprintf(stderr, "jobs: the -p and -l options are "
						"mutually exclusive\n");
				return EXIT_FAILURE;
			}
			pgids = true;
			break;
		case 'p':
			if (pgids) {
				fprintf(stderr, "jobs: the -p and -l options are "
						"mutually exclusive\n");
				return EXIT_FAILURE;
			}
			pids = true;
			break;
		default:
			fprintf(stderr, "jobs: unknown option -- %c\n", mrsh_optopt);
			fprintf(stderr, jobs_usage);
			return EXIT_FAILURE;
		}
	}

	struct mrsh_job *current = job_by_id(state, "%+", false);

	for (size_t i = 0; i < state->jobs.len; i++) {
		struct mrsh_job *job = state->jobs.data[i];
		if (job_poll(job) >= 0) {
			continue;
		}
		if (pids) {
			for (size_t j = 0; j < job->processes.len; ++j) {
				struct process *proc = job->processes.data[j];
				printf("%d\n", proc->pid);
			}
		} else if (pgids) {
			char *cmd = mrsh_node_format(job->node);
			printf("[%d] %c %d %s %s\n", job->job_id,
					job == current ? '+' : ' ', job->pgid,
					job_state_str(job), cmd);
			free(cmd);
		} else {
			char *cmd = mrsh_node_format(job->node);
			printf("[%d] %c %s %s\n", job->job_id, job == current ? '+' : ' ',
					job_state_str(job), cmd);
			free(cmd);
		}
	}

	return EXIT_SUCCESS;
}
