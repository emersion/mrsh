#include <assert.h>
#include "shell.h"

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
}

static struct task *handle_simple_command(struct mrsh_simple_command *sc) {
	struct task *task = task_builtin_create(sc);
	if (task != NULL) {
		return task;
	}

	return task_process_create(sc);
}

static struct task *handle_node(struct mrsh_node *node);

static struct task *handle_command_list_array(struct mrsh_array *array) {
	struct task *task_list = task_list_create();

	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		struct task *child = handle_node(list->node);
		if (list->ampersand) {
			child = task_bg_create(child);
		}
		task_list_add(task_list, child);
	}

	return task_list;
}

static struct task *handle_command(struct mrsh_command *cmd);

static struct task *handle_if_clause(struct mrsh_if_clause *ic) {
	struct task *condition = handle_command_list_array(&ic->condition);
	struct task *body = handle_command_list_array(&ic->body);
	struct task *else_part = NULL;
	if (ic->else_part != NULL) {
		else_part = handle_command(ic->else_part);
	}
	return task_if_clause_create(condition, body, else_part);
}

static struct task *handle_command(struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		assert(sc != NULL);
		return handle_simple_command(sc);
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		assert(bg != NULL);
		return handle_command_list_array(&bg->body);
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		assert(ic != NULL);
		return handle_if_clause(ic);
	}
	assert(false);
}

static struct task *handle_pipeline(struct mrsh_pipeline *pl) {
	struct task *task_pipeline = task_pipeline_create();

	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		task_pipeline_add(task_pipeline, handle_command(cmd));
	}

	return task_pipeline;
}

static struct task *handle_binop(struct mrsh_binop *binop) {
	struct task *left = handle_node(binop->left);
	struct task *right = handle_node(binop->right);
	return task_binop_create(binop->type, left, right);
}

static struct task *handle_node(struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl != NULL);
		return handle_pipeline(pl);
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		assert(binop != NULL);
		return handle_binop(binop);
	}
	assert(false);
}

int mrsh_run_command_list(struct mrsh_state *state,
		struct mrsh_command_list *list) {
	struct task *task = handle_node(list->node);

	struct context ctx = {
		.state = state,
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	return task_run(task, &ctx);
}

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	struct task *task = handle_command_list_array(&prog->body);

	struct context ctx = {
		.state = state,
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	return task_run(task, &ctx);
}
