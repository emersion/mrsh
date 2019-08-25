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
	int job_id;
	struct termios term_modes; // only valid if stopped
	struct mrsh_state *state;
	struct mrsh_array processes; // struct process *
};

/**
 * Create a new job. It will start in the background by default.
 */
struct mrsh_job *job_create(struct mrsh_state *state);
void job_destroy(struct mrsh_job *job);
/**
 * Add a process to the job. This puts the process into the job's process
 * group. This has to be done both in the parent and in the child to prevent
 * race conditions.
 *
 * If the job doesn't have a process group (because it's empty), then a new
 * process group is created.
 */
void job_add_process(struct mrsh_job *job, struct process *proc);
/**
 * Polls the job's current status without blocking. Returns:
 * - TASK_STATUS_WAIT if the job is running (ie. one or more processes are
 *   running)
 * - TASK_STATUS_STOPPED if the job is stopped (ie. one or more processes are
 *   stopped, all the others are terminated)
 * - An integer >= 0 if the job has terminated (ie. all processes have
 *   terminated)
 */
int job_poll(struct mrsh_job *job);
/**
 * Wait for the completion of the job.
 */
int job_wait(struct mrsh_job *job);
/**
 * Wait for the completion of the process.
 */
int job_wait_process(struct process *proc);
/**
 * Put the job in the foreground or in the background. If the job is stopped and
 * cont is set to true, it will be continued.
 *
 * It is illegal to put a job in the foreground if another job is already in the
 * foreground.
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
/**
 * Look up a job by its XBD Job Control Job ID.
 */
struct mrsh_job *job_by_id(struct mrsh_state *state, const char *id);

#endif
