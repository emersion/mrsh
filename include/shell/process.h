#ifndef SHELL_PROCESS_H
#define SHELL_PROCESS_H

#include <stdbool.h>
#include <sys/types.h>

/**
 * A child process.
 */
struct process {
	pid_t pid;
	bool stopped;
	bool finished;
	int stat;
};

void process_init(struct process *process, pid_t pid);
void process_finish(struct process *process);
int process_poll(struct process *process);
void process_notify(pid_t pid, int stat);

#endif
