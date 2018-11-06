#include "shell/task.h"
#include <mrsh/hashtable.h>
#include <stdlib.h>

struct task_function_definition {
	struct task task;
	const char *name;
	const struct mrsh_command *body;
};

static void task_function_definition_destroy(struct task *task) {
	struct task_function_definition *tfn =
		(struct task_function_definition *)task;
	free(tfn);
}

static int task_function_definition_poll(
		struct task *task, struct context *ctx) {
	struct task_function_definition *tfn =
		(struct task_function_definition *)task;
	struct mrsh_function *fn = calloc(1, sizeof(struct mrsh_function));
	fn->body = mrsh_command_copy(tfn->body);
	struct mrsh_function *oldfn =
		mrsh_hashtable_set(&ctx->state->functions, tfn->name, fn);
	function_destroy(oldfn);
	return 0;
}

static const struct task_interface task_function_definition_impl = {
	.destroy = task_function_definition_destroy,
	.poll = task_function_definition_poll,
};

struct task *task_function_definition_create(const char *name,
		const struct mrsh_command *body) {
	struct task_function_definition *tfn =
		calloc(1, sizeof(struct task_function_definition));
	task_init(&tfn->task, &task_function_definition_impl);
	tfn->name = name;
	tfn->body = body;
	return (struct task *)tfn;
}
