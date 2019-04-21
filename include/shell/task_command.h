#ifndef SHELL_TASK_COMMAND_H
#define SHELL_TASK_COMMAND_H

#include "shell/process.h"
#include "shell/task.h"

enum task_command_type {
	TASK_COMMAND_PROCESS,
	TASK_COMMAND_BUILTIN,
	TASK_COMMAND_FUNCTION,
};

struct task_command {
	struct task task;
	struct mrsh_simple_command *sc;
	bool started;
	enum task_command_type type;
	struct mrsh_array args;

	// if a process
	struct process *process;

	// if a function
	const struct mrsh_function *fn_def;
	struct task *fn_task;
};

int task_process_poll(struct task *task, struct context *ctx);
int task_builtin_poll(struct task *task, struct context *ctx);
int task_function_poll(struct task *task, struct context *ctx);

#endif
