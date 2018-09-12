#include <stdlib.h>
#include "shell/task.h"

struct task_list {
	struct task task;
	struct mrsh_array children;
	size_t current;
};

static void task_list_destroy(struct task *task) {
	struct task_list *tl = (struct task_list *)task;
	for (size_t i = 0; i < tl->children.len; ++i) {
		struct task *child = tl->children.data[i];
		task_destroy(child);
	}
	mrsh_array_finish(&tl->children);
	free(tl);
}

static int task_list_poll(struct task *task, struct context *ctx) {
	struct task_list *tl = (struct task_list *)task;

	int ret = 0;
	while (tl->current < tl->children.len) {
		struct task *child = tl->children.data[tl->current];

		ret = task_poll(child, ctx);
		if (ret < 0) {
			return ret;
		}

		++tl->current;
	}

	return ret;
}

static const struct task_interface task_list_impl = {
	.destroy = task_list_destroy,
	.poll = task_list_poll,
};

struct task *task_list_create(void) {
	struct task_list *tl = calloc(1, sizeof(struct task_list));
	task_init(&tl->task, &task_list_impl);
	return &tl->task;
}

void task_list_add(struct task *task, struct task *child) {
	struct task_list *tl = (struct task_list *)task;
	mrsh_array_add(&tl->children, child);
}
