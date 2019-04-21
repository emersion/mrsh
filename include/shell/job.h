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
 *
 * In practice, a single job is also created when executing an asynchronous
 * command list.
 *
 * This object is guaranteed to be valid until either:
 * - The job terminates
 * - The shell is destroyed
 */
struct mrsh_job {
	pid_t pgid;
	struct termios term_modes; // only valid if stopped
	struct mrsh_state *state;
	struct mrsh_array processes; // struct process *
};

/**
 * Create a new job with the provided process group ID.
 */
struct mrsh_job *job_create(struct mrsh_state *state, pid_t pgid);
void job_destroy(struct mrsh_job *job);
void job_add_process(struct mrsh_job *job, struct process *proc);
/**
 * Check whether all child processes have terminated. If there are no child
 * processes in this job, returns true.
 */
bool job_terminated(struct mrsh_job *job);
/**
 * Check whether there is at least one stopped child process and all others
 * have terminated.
 */
bool job_stopped(struct mrsh_job *job);
/**
 * Wait for the completion of the job.
 */
int job_wait(struct mrsh_job *job);
/**
 * Put the job in the foreground or in the background. If the job is stopped and
 * cont is set to true, it will be continued.
 */
bool job_set_foreground(struct mrsh_job *job, bool foreground, bool cont);

/**
 * Initialize a child process state.
 */
bool init_job_child_process(struct mrsh_state *state);
/**
 * Update the shell's state with a child process status.
 */
void update_job(struct mrsh_state *state, pid_t pid, int stat);

#endif
