#ifndef SHELL_JOB_H
#define SHELL_JOB_H

#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>

/**
 * A job is a group of processes. When job control is enabled, jobs can be:
 *
 * - Put in the foreground or in the background
 * - Stopped and continued
 *
 * The shell will typically wait for the foreground job to finish or to stop
 * before displaying its prompt.
 */
struct job {
	pid_t pgid;
	struct mrsh_state *state;
	struct termios term_modes;
	// TODO: list of processes
};

// TODO: these shouldn't be globals
extern struct mrsh_array jobs; // struct job *

struct job *job_foreground(void);
struct job *job_create(struct mrsh_state *state);
void job_destroy(struct job *job);
void job_add_process(struct job *job, pid_t pid);
bool job_set_foreground(struct job *job, bool foreground);
bool job_continue(struct job *job);
bool job_wait(struct job *job);

bool job_child_init(struct mrsh_state *state, bool foreground);
void job_notify(pid_t pid, int stat);

#endif
