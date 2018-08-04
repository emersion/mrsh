#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

struct task_token {
	struct task task;
	struct mrsh_token **token_ptr;
};

static void task_token_swap(struct task_token *tt,
		struct mrsh_token *new_token) {
	mrsh_token_destroy(*tt->token_ptr);
	*tt->token_ptr = new_token;
}

static int task_token_poll(struct task *task, struct context *ctx) {
	struct task_token *tt = (struct task_token *)task;
	struct mrsh_token *token = *tt->token_ptr;

	switch (token->type) {
	case MRSH_TOKEN_STRING:
		return 0; // Nothing to do
	case MRSH_TOKEN_PARAMETER:;
		struct mrsh_token_parameter *tp = mrsh_token_get_parameter(token);
		assert(tp != NULL);
		const char *value =
			mrsh_hashtable_get(&ctx->state->variables, tp->name);
		if (value == NULL) {
			value = "";
		}
		struct mrsh_token_string *ts =
			mrsh_token_string_create(strdup(value), false);
		task_token_swap(tt, &ts->token);
		return 0;
	case MRSH_TOKEN_LIST:
		assert(0);
	}
	assert(0);
}

static const struct task_interface task_token_impl = {
	.poll = task_token_poll,
};

struct task *task_token_create(struct mrsh_token **token_ptr) {
	struct mrsh_token *token = *token_ptr;

	if (token->type == MRSH_TOKEN_LIST) {
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		struct task *task_list = task_list_create();
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token **child_ptr =
				(struct mrsh_token **)&tl->children.data[i];
			task_list_add(task_list, task_token_create(child_ptr));
		}
		return task_list;
	}

	struct task_token *tt = calloc(1, sizeof(struct task_token));
	task_init(&tt->task, &task_token_impl);
	tt->token_ptr = token_ptr;
	return &tt->task;
}
