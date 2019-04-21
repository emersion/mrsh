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
	int stat;
};

struct process *process_create(struct mrsh_state *state, pid_t pid);
void process_destroy(struct process *process);
int process_poll(struct process *process);

void update_process(struct mrsh_state *state, pid_t pid, int stat);

#endif
