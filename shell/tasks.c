#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"
#include "shell/tasks.h"

struct task *task_for_simple_command(struct mrsh_simple_command *sc) {
	struct task *task_list = task_list_create();

	for (size_t i = 0; i < sc->assignments.len; ++i) {
		struct mrsh_assignment *assign = sc->assignments.data[i];
		task_list_add(task_list,
			task_word_create(&assign->value, TILDE_EXPANSION_ASSIGNMENT));
	}

	if (sc->name == NULL) {
		task_list_add(task_list, task_assignment_create(&sc->assignments));
		return task_list;
	}

	task_list_add(task_list,
		task_word_create(&sc->name, TILDE_EXPANSION_NAME));

	for (size_t i = 0; i < sc->arguments.len; ++i) {
		struct mrsh_word **arg_ptr =
			(struct mrsh_word **)&sc->arguments.data[i];
		task_list_add(task_list,
			task_word_create(arg_ptr, TILDE_EXPANSION_NAME));
	}

	for (size_t i = 0; i < sc->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
		task_list_add(task_list,
			task_word_create(&redir->name, TILDE_EXPANSION_NAME));
		for (size_t j = 0; j < redir->here_document.len; ++j) {
			struct mrsh_word **line_word_ptr =
				(struct mrsh_word **)&redir->here_document.data[j];
			task_list_add(task_list,
				task_word_create(line_word_ptr, TILDE_EXPANSION_NAME));
		}
	}

	task_list_add(task_list, task_command_create(sc));

	return task_list;
}

struct task *task_for_command_list_array(struct mrsh_array *array) {
	struct task *task_list = task_list_create();

	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		struct task *child = task_for_node(list->node);
		if (list->ampersand) {
			child = task_async_create(child);
		}
		task_list_add(task_list, child);
	}

	return task_list;
}

struct task *task_for_if_clause(struct mrsh_if_clause *ic) {
	struct task *condition = task_for_command_list_array(&ic->condition);
	struct task *body = task_for_command_list_array(&ic->body);
	struct task *else_part = NULL;
	if (ic->else_part != NULL) {
		else_part = task_for_command(ic->else_part);
	}
	return task_if_clause_create(condition, body, else_part);
}

struct task *task_for_command(struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		return task_for_simple_command(sc);
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		return task_for_command_list_array(&bg->body);
	case MRSH_SUBSHELL:;
		struct mrsh_subshell *s = mrsh_command_get_subshell(cmd);
		return task_subshell_create(task_for_command_list_array(&s->body));
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		return task_for_if_clause(ic);
	case MRSH_LOOP_CLAUSE:
	case MRSH_FOR_CLAUSE:
	case MRSH_CASE_CLAUSE:
	case MRSH_FUNCTION_DEFINITION:
		assert(false); // TODO: implement this
	}
	assert(false);
}

struct task *task_for_pipeline(struct mrsh_pipeline *pl) {
	struct task *task_pipeline = task_pipeline_create();

	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		task_pipeline_add(task_pipeline, task_for_command(cmd));
	}

	return task_pipeline;
}

struct task *task_for_binop(struct mrsh_binop *binop) {
	struct task *left = task_for_node(binop->left);
	struct task *right = task_for_node(binop->right);
	return task_binop_create(binop->type, left, right);
}

struct task *task_for_node(struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		return task_for_pipeline(pl);
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		return task_for_binop(binop);
	}
	assert(false);
}
