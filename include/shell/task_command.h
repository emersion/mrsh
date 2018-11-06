#ifndef _SHELL_TASK_COMMAND_H
#define _SHELL_TASK_COMMAND_H

#include "shell/process.h"
#include "shell/task.h"

struct task_command {
	struct task task;
	struct mrsh_simple_command *sc;
	bool started;
	bool builtin;
	struct mrsh_array args;

	// if a process
	struct process process;
	// if a function
	const struct mrsh_function *fn_def;
	struct task *fn_task;
};

int task_process_poll(struct task *task, struct context *ctx);
int task_builtin_poll(struct task *task, struct context *ctx);
int task_function_poll(struct task *task, struct context *ctx);

#endif
