#include <assert.h>
#include <mrsh/builtin.h>
#include <stdlib.h>
#include "shell/task_command.h"
#include "shell/task.h"

static void task_command_destroy(struct task *task) {
	struct task_command *tc = (struct task_command *)task;
	mrsh_command_destroy(&tc->sc->command);
	for (size_t i = 0; i < tc->args.len; ++i) {
		free(tc->args.data[i]);
	}
	mrsh_array_finish(&tc->args);
	switch (tc->type) {
	case TASK_COMMAND_PROCESS:
		process_finish(&tc->process);
		break;
	case TASK_COMMAND_BUILTIN:
		break;
	case TASK_COMMAND_FUNCTION:
		task_destroy(tc->fn_task);
		break;
	}
	free(tc);
}

static void get_args(struct mrsh_array *args, struct mrsh_simple_command *sc,
		struct context *ctx) {
	struct mrsh_array fields = {0};
	const char *ifs = mrsh_env_get(ctx->state, "IFS", NULL);
	split_fields(&fields, sc->name, ifs);
	for (size_t i = 0; i < sc->arguments.len; ++i) {
		struct mrsh_word *word = sc->arguments.data[i];
		split_fields(&fields, word, ifs);
	}
	assert(fields.len > 0);

	if (ctx->state->options & MRSH_OPT_NOGLOB) {
		*args = fields;
	} else {
		expand_pathnames(args, &fields);
		for (size_t i = 0; i < fields.len; ++i) {
			free(fields.data[i]);
		}
		mrsh_array_finish(&fields);
	}

	assert(args->len > 0);
	mrsh_array_add(args, NULL);
}

static int task_command_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;
	struct mrsh_simple_command *sc = tc->sc;

	if (!tc->started) {
		get_args(&tc->args, sc, ctx);
		const char *argv_0 = (char *)tc->args.data[0];

		enum task_command_type type;
		tc->fn_def = mrsh_hashtable_get(&ctx->state->functions, argv_0);
		if (tc->fn_def != NULL) {
			type = TASK_COMMAND_FUNCTION;
		} else if (mrsh_has_builtin(argv_0)) {
			type = TASK_COMMAND_BUILTIN;
		} else {
			type = TASK_COMMAND_PROCESS;
		}
		tc->type = type;
	}

	switch (tc->type) {
	case TASK_COMMAND_PROCESS:
		return task_process_poll(task, ctx);
	case TASK_COMMAND_BUILTIN:
		return task_builtin_poll(task, ctx);
	case TASK_COMMAND_FUNCTION:
		return task_function_poll(task, ctx);
	}
}

static const struct task_interface task_command_impl = {
	.destroy = task_command_destroy,
	.poll = task_command_poll,
};

struct task *task_command_create(struct mrsh_simple_command *sc) {
	struct task_command *tc = calloc(1, sizeof(struct task_command));
	task_init(&tc->task, &task_command_impl);
	tc->sc = sc;
	return &tc->task;
}
