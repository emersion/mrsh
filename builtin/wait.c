#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include "builtin.h"
#include "shell/process.h"
#include "shell/shell.h"

struct wait_handle {
	pid_t pid;
	int status;
};

int builtin_wait(struct mrsh_state *state, int argc, char *argv[]) {
	int npids = argc - 1;
	if (npids == 0) {
		npids = state->processes.len;
	}
	struct wait_handle *pids = malloc(npids * sizeof(struct wait_handle));
	if (pids == NULL) {
		fprintf(stderr, "wait: unable to allocate pid list");
		return EXIT_FAILURE;
	}

	if (argc == 1) {
		/* All known processes */
		int _npids = 0;
		for (size_t j = 0; j < state->processes.len; ++j) {
			struct process *process = state->processes.data[j];
			if (process->terminated) {
				continue;
			}
			pids[_npids].pid = process->pid;
			pids[_npids].status = -1;
			++_npids;
		}
		npids = _npids;
	} else {
		for (int i = 1; i < argc; ++i) {
			if (argv[i][0] == '%') {
				struct mrsh_job *job = job_by_id(state, argv[i], true);
				if (!job) {
					goto failure;
				}
				pids[i - 1].pid = job->pgid;
				pids[i - 1].status = -1;
			} else {
				char *endptr;
				pid_t pid = (pid_t)strtol(argv[i], &endptr, 10);
				if (*endptr != '\0' || argv[i][0] == '\0') {
					fprintf(stderr, "wait: error parsing pid '%s'", argv[i]);
					goto failure;
				}
				if (pid <= 0) {
					fprintf(stderr, "wait: invalid process ID\n");
					goto failure;
				}
				pids[i - 1].pid = pid;
				pids[i - 1].status = -1;
				/* Check if this pid is known */
				bool found = false;
				for (size_t j = 0; j < state->processes.len; ++j) {
					struct process *process = state->processes.data[j];
					if (process->pid == pid) {
						if (process->terminated) {
							pids[i - 1].status = process->stat;
						}
						found = true;
						break;
					}
				}
				if (!found) {
					/* Unknown pids are assumed to have exited 127 */
					pids[i - 1].status = 127;
				}
			}
		}
	}

	for (int i = 0; i < npids; ++i) {
		int stat;
		pid_t waited = waitpid(pids[i].pid, &stat, 0);
		// TODO: update jobs internal state?
		if (waited == -1) {
			if (errno == ECHILD) {
				continue;
			}
			perror("wait");
			goto failure;
		}
		update_process(state, waited, stat);
		if (WIFEXITED(stat)) {
			pids[i].status = WEXITSTATUS(stat);
		} else {
			pids[i].status = 129;
		}
	}

	int status;
	if (argc == 1) {
		status = EXIT_SUCCESS;
	} else {
		status = pids[npids - 1].status;
	}

	free(pids);
	return status;

failure:
	free(pids);
	return EXIT_FAILURE;
}
