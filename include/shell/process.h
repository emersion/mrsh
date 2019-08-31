#ifndef SHELL_PROCESS_H
#define SHELL_PROCESS_H

#include <mrsh/shell.h>
#include <stdbool.h>
#include <sys/types.h>

/**
 * This struct is used to track child processes.
 *
 * This object is guaranteed to be valid until either:
 * - The process terminates
 * - The shell is destroyed
 */
struct process {
	pid_t pid;
	struct mrsh_state *state;
	bool stopped;
	bool terminated;
	int stat; // only valid if terminated
	int signal; // only valid if stopped is true
};

/**
 * Register a new process.
 */
struct process *process_create(struct mrsh_state *state, pid_t pid);
void process_destroy(struct process *process);
/**
 * Polls the process' current status without blocking. Returns:
 * - An integer >= 0 if the process has terminated
 * - TASK_STATUS_STOPPED if the process is stopped
 * - TASK_STATUS_WAIT if the process is running
 */
int process_poll(struct process *process);

/**
 * Update the shell's state with a child process status.
 */
void update_process(struct mrsh_state *state, pid_t pid, int stat);

#endif
