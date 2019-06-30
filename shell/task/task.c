#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <mrsh/ast.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/task.h"

static int run_subshell(struct context *ctx, struct mrsh_array *array) {
	struct process *process;
	pid_t pid = subshell_fork(ctx, &process);
	if (pid < 0) {
		return TASK_STATUS_ERROR;
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

		if (ctx->state->exit >= 0) {
			exit(ctx->state->exit);
		}
		exit(ret);
	}

	return job_wait_process(process);
}

static int run_if_clause(struct context *ctx, struct mrsh_if_clause *ic) {
	int ret = run_command_list_array(ctx, &ic->condition);
	if (ret < 0) {
		return ret;
	}

	if (ret == 0) {
		return run_command_list_array(ctx, &ic->body);
	} else {
		if (ic->else_part) {
			return run_command(ctx, ic->else_part);
		}
		return 0;
	}
}

static int run_loop_clause(struct context *ctx, struct mrsh_loop_clause *lc) {
	int loop_num = ++ctx->state->nloops;

	int loop_ret = 0;
	while (ctx->state->exit == -1) {
		int ret = run_command_list_array(ctx, &lc->condition);
		if (ret == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (ret < 0) {
			return ret;
		}

		bool break_loop;
		switch (lc->type) {
		case MRSH_LOOP_WHILE:
			break_loop = ret > 0;
			break;
		case MRSH_LOOP_UNTIL:
			break_loop = ret == 0;
			break;
		}
		if (break_loop) {
			break;
		}

		loop_ret = run_command_list_array(ctx, &lc->body);
		if (loop_ret == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (loop_ret < 0) {
			return loop_ret;
		}

		continue;

interrupt:
		if (ctx->state->nloops < loop_num) {
			loop_ret = TASK_STATUS_INTERRUPTED; // break to parent loop
			break;
		}
		switch (ctx->state->branch_control) {
		case MRSH_BRANCH_BREAK:
			break_loop = true;
			loop_ret = 0;
			break;
		case MRSH_BRANCH_CONTINUE:
			break;
		case MRSH_BRANCH_RETURN:
			assert(false);
		}
		if (break_loop) {
			break;
		}
	}

	--ctx->state->nloops;
	return loop_ret;
}

static int run_for_clause(struct context *ctx, struct mrsh_for_clause *fc) {
	int loop_num = ++ctx->state->nloops;

	int loop_ret = 0;
	size_t word_index = 0;
	while (ctx->state->exit == -1) {
		if (word_index == fc->word_list.len) {
			break;
		}

		// TODO: this mutates the AST
		struct mrsh_word **word_ptr =
			(struct mrsh_word **)&fc->word_list.data[word_index++];
		int ret = run_word(ctx, word_ptr, TILDE_EXPANSION_NAME);
		if (ret == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (ret < 0) {
			return ret;
		}
		struct mrsh_word_string *ws = mrsh_word_get_string(*word_ptr);
		mrsh_env_set(ctx->state, fc->name, ws->str, MRSH_VAR_ATTRIB_NONE);

		loop_ret = run_command_list_array(ctx, &fc->body);
		if (loop_ret == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (loop_ret < 0) {
			return loop_ret;
		}

		continue;

interrupt:
		if (ctx->state->nloops < loop_num) {
			loop_ret = TASK_STATUS_INTERRUPTED; // break to parent loop
			break;
		}
		bool break_loop = false;
		switch (ctx->state->branch_control) {
		case MRSH_BRANCH_BREAK:
			break_loop = true;
			loop_ret = 0;
			break;
		case MRSH_BRANCH_CONTINUE:
			break;
		case MRSH_BRANCH_RETURN:
			assert(false);
		}
		if (break_loop) {
			break;
		}
	}

	--ctx->state->nloops;
	return loop_ret;
}

static int run_case_clause(struct context *ctx, struct mrsh_case_clause *cc) {
	struct mrsh_word *word = mrsh_word_copy(cc->word);
	int ret = run_word(ctx, &word, TILDE_EXPANSION_NAME);
	if (ret < 0) {
		mrsh_word_destroy(word);
		return ret;
	}
	char *word_str = mrsh_word_str(word);
	mrsh_word_destroy(word);

	int case_ret = 0;
	for (size_t i = 0; i < cc->items.len; ++i) {
		struct mrsh_case_item *ci = cc->items.data[i];

		bool selected = false;
		for (size_t j = 0; j < ci->patterns.len; ++j) {
			// TODO: this mutates the AST
			struct mrsh_word **word_ptr =
				(struct mrsh_word **)&ci->patterns.data[j];
			int ret = run_word(ctx, word_ptr, TILDE_EXPANSION_NAME);
			if (ret < 0) {
				return ret;
			}
			struct mrsh_word_string *ws = mrsh_word_get_string(*word_ptr);
			if (fnmatch(ws->str, word_str, 0) == 0) {
				selected = true;
				break;
			}
		}

		if (selected) {
			case_ret = run_command_list_array(ctx, &ci->body);
			break;
		}
	}

	free(word_str);
	return case_ret;
}

static int run_function_definition(struct context *ctx,
		struct mrsh_function_definition *fnd) {
	struct mrsh_function *fn = calloc(1, sizeof(struct mrsh_function));
	fn->body = mrsh_command_copy(fnd->body);
	struct mrsh_function *old_fn =
		mrsh_hashtable_set(&ctx->state->functions, fnd->name, fn);
	mrsh_function_destroy(old_fn);
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
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		return run_if_clause(ctx, ic);
	case MRSH_LOOP_CLAUSE:;
		struct mrsh_loop_clause *lc = mrsh_command_get_loop_clause(cmd);
		return run_loop_clause(ctx, lc);
	case MRSH_FOR_CLAUSE:;
		struct mrsh_for_clause *fc = mrsh_command_get_for_clause(cmd);
		return run_for_clause(ctx, fc);
	case MRSH_CASE_CLAUSE:;
		struct mrsh_case_clause *cc =
			mrsh_command_get_case_clause(cmd);
		return run_case_clause(ctx, cc);
	case MRSH_FUNCTION_DEFINITION:;
		struct mrsh_function_definition *fnd =
			mrsh_command_get_function_definition(cmd);
		return run_function_definition(ctx, fnd);
	}
	assert(false);
}

int run_node(struct context *ctx, struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		return run_pipeline(ctx, pl);
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
			struct context child_ctx = *ctx;
			child_ctx.background = true;

			pid_t pid = subshell_fork(ctx, NULL);
			if (pid < 0) {
				return TASK_STATUS_ERROR;
			} else if (pid == 0) {
				if (!(child_ctx.state->options & MRSH_OPT_MONITOR)) {
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

				int ret = run_node(&child_ctx, list->node);
				if (ret < 0) {
					exit(127);
				}
				exit(ret);
			}
			ret = 0;
		} else {
			ret = run_node(ctx, list->node);
			if (ret < 0) {
				return ret;
			}
		}
		if (ret >= 0) {
			ctx->state->last_status = ret;
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
	int last_status = state->last_status;
	int ret = run_word(&ctx, word, TILDE_EXPANSION_NAME);
	state->last_status = last_status;
	return ret;
}
