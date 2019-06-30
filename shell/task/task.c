#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mrsh/ast.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"

static int run_subshell(struct context *ctx, struct mrsh_array *array) {
	// Start a subshell
	pid_t pid = subshell_fork(ctx, NULL);
	if (pid < 0) {
		return -1;
	} else if (pid == 0) {
		if (!(ctx->state->options & MRSH_OPT_MONITOR)) {
			// If job control is disabled, stdin is /dev/null
			int fd = open("/dev/null", O_CLOEXEC | O_RDONLY);
			if (fd < 0) {
				fprintf(stderr, "failed to open /dev/null: %s\n",
					strerror(errno));
				exit(1);
			}
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		int ret = run_command_list_array(ctx, array);
		if (ret < 0) {
			exit(127);
		}

		exit(ret);
	}

	return 0;
}

int run_command(struct context *ctx, struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		return run_simple_command(ctx, sc);
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		return run_command_list_array(ctx, &bg->body);
	case MRSH_SUBSHELL:;
		struct mrsh_subshell *s = mrsh_command_get_subshell(cmd);
		return run_subshell(ctx, &s->body);
	/*case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		return run_if_clause(ic);
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		return run_loop_clause(lc);
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		return run_for_clause(fc);
	case MRSH_CASE_CLAUSE:;
		struct mrsh_case_clause *cc =
			mrsh_command_get_case_clause(cmd);
		return run_case_clause(cc);
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fn =
			mrsh_command_get_function_definition(cmd);
		return run_function_definition(fn);*/
	default:
		assert(false);
	}
	assert(false);
}

int run_node(struct context *ctx, struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl->commands.len == 1); // TODO
		/*for (size_t i = 0; i < pl->commands.len; ++i) {
			struct mrsh_command *cmd = pl->commands.data[i];
			task_pipeline_add(task_pipeline, task_for_command(cmd));
		}*/
		return run_command(ctx, pl->commands.data[0]);
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		int left_status = run_node(ctx, binop->left);
		switch (binop->type) {
		case MRSH_BINOP_AND:
			if (left_status != 0) {
				return left_status;
			}
			break;
		case MRSH_BINOP_OR:
			if (left_status == 0) {
				return 0;
			}
			break;
		}
		return run_node(ctx, binop->right);
	}
	assert(false);
}

int run_command_list_array(struct context *ctx, struct mrsh_array *array) {
	int ret = 0;
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		if (list->ampersand) {
			assert(false); // TODO
		} else {
			ret = run_node(ctx, list->node);
		}
	}
	return ret;
}

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	struct context ctx = { .state = state };
	return run_command_list_array(&ctx, &prog->body);
}

int mrsh_run_word(struct mrsh_state *state, struct mrsh_word **word) {
	struct context ctx = { .state = state };
	return run_word(&ctx, word, TILDE_EXPANSION_NAME);
}
