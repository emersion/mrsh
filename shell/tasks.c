#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"
#include "shell/tasks.h"

static struct mrsh_simple_command *copy_simple_command(
		const struct mrsh_simple_command *sc) {
	struct mrsh_command *cmd = mrsh_command_copy(&sc->command);
	return mrsh_command_get_simple_command(cmd);
}

static void expand_assignments(struct task *task_list,
		struct mrsh_array *assignments) {
	for (size_t i = 0; i < assignments->len; ++i) {
		struct mrsh_assignment *assign = assignments->data[i];
		task_list_add(task_list,
			task_word_create(&assign->value, TILDE_EXPANSION_ASSIGNMENT));
	}
}

struct task *task_for_simple_command(struct mrsh_simple_command *sc) {
	struct task *task_list = task_list_create();

	if (sc->name == NULL) {
		// Copy each assignment from the AST, because during expansion and
		// substitution we'll mutate the tree
		struct mrsh_array assignments = {0};
		mrsh_array_reserve(&assignments, sc->assignments.len);
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			mrsh_array_add(&assignments, mrsh_assignment_copy(assign));
		}

		expand_assignments(task_list, &assignments);

		task_list_add(task_list, task_assignment_create(&assignments));
		return task_list;
	}

	// Copy the command from the AST, because during expansion and substitution
	// we'll mutate the tree
	sc = copy_simple_command(sc);

	task_list_add(task_list,
		task_word_create(&sc->name, TILDE_EXPANSION_NAME));

	expand_assignments(task_list, &sc->assignments);

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

struct task *task_for_loop_clause(struct mrsh_loop_clause *lc) {
	return task_loop_clause_create(&lc->condition, &lc->body,
			lc->type == MRSH_LOOP_UNTIL);
}

struct task *task_for_for_clause(struct mrsh_for_clause *fc) {
	return task_for_clause_create(fc->name, &fc->word_list, &fc->body);
}

struct task *task_for_function_definition(struct mrsh_function_definition *fn) {
	return task_function_definition_create(fn->name, fn->body);
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
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		return task_for_loop_clause(lc);
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		return task_for_for_clause(fc);
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fn =
			mrsh_command_get_function_definition(cmd);
		return task_for_function_definition(fn);
	case MRSH_CASE_CLAUSE:
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
