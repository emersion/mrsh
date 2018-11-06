#include "shell/task_command.h"
#include "shell/tasks.h"

static bool task_function_start(struct task_command *tc, struct context *ctx) {
	int argc = tc->args.len - 1;
	const char **argv = (const char **)tc->args.data;
	mrsh_push_args(ctx->state, argc, argv);
	tc->fn_task = task_for_command(tc->fn_def->body);
	return tc->fn_task != NULL;
}

int task_function_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;

	if (!tc->started) {
		if (!task_function_start(tc, ctx)) {
			mrsh_pop_args(ctx->state);
			return TASK_STATUS_ERROR;
		}
		tc->started = true;
	}

	int ret = task_poll(tc->fn_task, ctx);
	if (ret >= 0) {
		mrsh_pop_args(ctx->state);
	}
	return ret;
}
