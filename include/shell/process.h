#ifndef SHELL_PROCESS_H
#define SHELL_PROCESS_H

#include <mrsh/shell.h>
#include <stdbool.h>
#include <sys/types.h>

/**
 * This struct is used to track child processes.
 */
struct process {
	pid_t pid;
	struct mrsh_state *state;
	bool finished;
	int stat;
};

void process_init(struct process *process, struct mrsh_state *state, pid_t pid);
void process_finish(struct process *process);
int process_poll(struct process *process);
void process_notify(struct mrsh_state *state, pid_t pid, int stat);

#endif
