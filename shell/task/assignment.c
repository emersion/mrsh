#include <string.h>
#include <stdlib.h>
#include "shell/task.h"

struct task_assignment {
	struct task task;
	struct mrsh_array assignments;
};

static void task_assignment_destroy(struct task *task) {
	struct task_assignment *ta = (struct task_assignment *)task;
	for (size_t i = 0; i < ta->assignments.len; ++i) {
		mrsh_assignment_destroy(ta->assignments.data[i]);
	}
	mrsh_array_finish(&ta->assignments);
	free(ta);
}

static int task_assignment_poll(struct task *task, struct context *ctx) {
	struct task_assignment *ta = (struct task_assignment *)task;

	for (size_t i = 0; i < ta->assignments.len; ++i) {
		struct mrsh_assignment *assign = ta->assignments.data[i];
		char *new_value = mrsh_word_str(assign->value);
		uint32_t attribs = MRSH_VAR_ATTRIB_NONE;
		if ((ctx->state->options & MRSH_OPT_ALLEXPORT)) {
			attribs = MRSH_VAR_ATTRIB_EXPORT;
		}
		uint32_t prev_attribs = 0;
		if (mrsh_env_get(ctx->state, assign->name, &prev_attribs) != NULL
				&& (prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
			free(new_value);
			fprintf(stderr, "cannot modify readonly variable %s\n",
					assign->name);
			task->status = 1;
			return TASK_STATUS_ERROR;
		}
		mrsh_env_set(ctx->state, assign->name, new_value, attribs);
		free(new_value);
	}

	return 0;
}

static const struct task_interface task_assignment_impl = {
	.destroy = task_assignment_destroy,
	.poll = task_assignment_poll,
};

struct task *task_assignment_create(struct mrsh_array *assignments) {
	struct task_assignment *ta = calloc(1, sizeof(struct task_assignment));
	task_init(&ta->task, &task_assignment_impl);
	ta->assignments = *assignments;
	return &ta->task;
}
