#ifndef SHELL_JOB_H
#define SHELL_JOB_H

#include <mrsh/array.h>
#include <stdbool.h>
#include <sys/types.h>

struct mrsh_state;
struct process;

/**
 * A job is a set of processes, comprising a shell pipeline, and any processes
 * descended from it, that are all in the same process group.
 */
struct mrsh_job {
	pid_t pgid;
	struct mrsh_state *state;
	bool finished;
	struct mrsh_array processes; // struct process *
};

struct mrsh_job *job_create(struct mrsh_state *state, pid_t pgid);
void job_destroy(struct mrsh_job *job);
void job_add_process(struct mrsh_job *job, struct process *proc);

bool job_init_process(struct mrsh_state *state);
void job_notify(struct mrsh_state *state, pid_t pid, int stat);

#endif
