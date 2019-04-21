#ifndef SHELL_JOB_H
#define SHELL_JOB_H

#include <mrsh/array.h>
#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

struct mrsh_state;
struct process;

/**
 * A job is a set of processes, comprising a shell pipeline, and any processes
 * descended from it, that are all in the same process group.
 */
struct mrsh_job {
	pid_t pgid;
	struct termios term_modes;
	struct mrsh_state *state;
	struct mrsh_array processes; // struct process *
};

struct mrsh_job *job_create(struct mrsh_state *state, pid_t pgid);
void job_destroy(struct mrsh_job *job);
void job_add_process(struct mrsh_job *job, struct process *proc);
bool job_terminated(struct mrsh_job *job);
bool job_stopped(struct mrsh_job *job);
int job_wait(struct mrsh_job *job);
/**
 * Put the job in the foreground or in the background. If the job is stopped and
 * cont is set to true, it will be continued.
 */
bool job_set_foreground(struct mrsh_job *job, bool foreground, bool cont);

bool init_job_child_process(struct mrsh_state *state);
void update_job(struct mrsh_state *state, pid_t pid, int stat);

#endif
