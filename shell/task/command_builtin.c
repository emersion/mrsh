#include <assert.h>
#include <mrsh/builtin.h>
#include "shell/task_command.h"

int task_builtin_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;

	assert(!tc->started);
	tc->started = true;

	// TODO: redirections
	int argc = tc->args.len - 1;
	char **argv = (char **)tc->args.data;
	return mrsh_run_builtin(ctx->state, argc, argv);
}
