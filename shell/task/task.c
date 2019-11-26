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

static int run_subshell(struct mrsh_context *ctx, struct mrsh_array *array) {
	struct mrsh_state_priv *priv = state_get_priv(ctx->state);

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return TASK_STATUS_ERROR;
	} else if (pid == 0) {
		priv->child = true;

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

	struct mrsh_process *proc = process_create(ctx->state, pid);
	return job_wait_process(proc);
}

static int run_if_clause(struct mrsh_context *ctx, struct mrsh_if_clause *ic) {
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

static int run_loop_clause(struct mrsh_context *ctx, struct mrsh_loop_clause *lc) {
	struct mrsh_call_frame_priv *frame_priv =
		call_frame_get_priv(ctx->state->frame);
	int loop_num = ++frame_priv->nloops;

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
		if (frame_priv->nloops < loop_num) {
			loop_ret = TASK_STATUS_INTERRUPTED; // break to parent loop
			break;
		}
		switch (frame_priv->branch_control) {
		case MRSH_BRANCH_BREAK:
		case MRSH_BRANCH_RETURN:
		case MRSH_BRANCH_EXIT:
			break_loop = true;
			loop_ret = 0;
			break;
		case MRSH_BRANCH_CONTINUE:
			break;
		}
		if (break_loop) {
			break;
		}
	}

	--frame_priv->nloops;
	return loop_ret;
}

static int run_for_clause(struct mrsh_context *ctx, struct mrsh_for_clause *fc) {
	struct mrsh_call_frame_priv *frame_priv =
		call_frame_get_priv(ctx->state->frame);
	int loop_num = ++frame_priv->nloops;

	struct mrsh_array fields = {0};
	for (size_t i = 0; i < fc->word_list.len; i++) {
		struct mrsh_word *word = fc->word_list.data[i];
		int ret = expand_word(ctx, word, &fields);
		if (ret < 0) {
			return ret;
		}
	}

	int loop_ret = 0;
	size_t word_index = 0;
	while (ctx->state->exit == -1) {
		if (word_index == fields.len) {
			break;
		}

		mrsh_env_set(ctx->state, fc->name, fields.data[word_index],
			MRSH_VAR_ATTRIB_NONE);
		word_index++;

		loop_ret = run_command_list_array(ctx, &fc->body);
		if (loop_ret == TASK_STATUS_INTERRUPTED) {
			goto interrupt;
		} else if (loop_ret < 0) {
			return loop_ret;
		}

		continue;

interrupt:
		if (frame_priv->nloops < loop_num) {
			loop_ret = TASK_STATUS_INTERRUPTED; // break to parent loop
			break;
		}
		bool break_loop = false;
		switch (frame_priv->branch_control) {
		case MRSH_BRANCH_BREAK:
		case MRSH_BRANCH_RETURN:
		case MRSH_BRANCH_EXIT:
			break_loop = true;
			loop_ret = 0;
			break;
		case MRSH_BRANCH_CONTINUE:
			break;
		}
		if (break_loop) {
			break;
		}
	}

	for (size_t i = 0; i < fields.len; i++) {
		free(fields.data[i]);
	}
	mrsh_array_finish(&fields);

	--frame_priv->nloops;
	return loop_ret;
}

static int run_case_clause(struct mrsh_context *ctx, struct mrsh_case_clause *cc) {
	struct mrsh_word *word = mrsh_word_copy(cc->word);
	expand_tilde(ctx->state, &word, false);
	int ret = run_word(ctx, &word);
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
			expand_tilde(ctx->state, word_ptr, false);
			int ret = run_word(ctx, word_ptr);
			if (ret < 0) {
				return ret;
			}
			char *pattern = word_to_pattern(*word_ptr);
			if (pattern != NULL) {
				selected = fnmatch(pattern, word_str, 0) == 0;
				free(pattern);
			} else {
				char *str = mrsh_word_str(*word_ptr);
				selected = strcmp(str, word_str) == 0;
				free(str);
			}
			if (selected) {
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

static int run_function_definition(struct mrsh_context *ctx,
		struct mrsh_function_definition *fnd) {
	struct mrsh_state_priv *priv = state_get_priv(ctx->state);

	struct mrsh_function *fn = calloc(1, sizeof(struct mrsh_function));
	fn->body = mrsh_command_copy(fnd->body);
	struct mrsh_function *old_fn =
		mrsh_hashtable_set(&priv->functions, fnd->name, fn);
	function_destroy(old_fn);
	return 0;
}

int run_command(struct mrsh_context *ctx, struct mrsh_command *cmd) {
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

int run_and_or_list(struct mrsh_context *ctx, struct mrsh_and_or_list *and_or_list) {
	switch (and_or_list->type) {
	case MRSH_AND_OR_LIST_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_and_or_list_get_pipeline(and_or_list);
		return run_pipeline(ctx, pl);
	case MRSH_AND_OR_LIST_BINOP:;
		struct mrsh_binop *binop = mrsh_and_or_list_get_binop(and_or_list);
		int left_status = run_and_or_list(ctx, binop->left);
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
		return run_and_or_list(ctx, binop->right);
	}
	assert(false);
}

/**
 * Put the process into its job's process group. This has to be done both in the
 * parent and the child because of potential race conditions.
 */
static struct mrsh_process *init_async_child(struct mrsh_context *ctx, pid_t pid) {
	struct mrsh_process *proc = process_create(ctx->state, pid);

	if (ctx->state->options & MRSH_OPT_MONITOR) {
		job_add_process(ctx->job, proc);
	}

	return proc;
}

int run_command_list_array(struct mrsh_context *ctx, struct mrsh_array *array) {
	struct mrsh_state *state = ctx->state;
	struct mrsh_state_priv *priv = state_get_priv(state);

	int ret = 0;
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		if (list->ampersand) {
			struct mrsh_context child_ctx = *ctx;
			child_ctx.background = true;
			if (child_ctx.job == NULL) {
				child_ctx.job = job_create(state, &list->node);
			}

			pid_t pid = fork();
			if (pid < 0) {
				perror("fork");
				return TASK_STATUS_ERROR;
			} else if (pid == 0) {
				ctx = NULL; // Use child_ctx instead
				priv->child = true;

				init_async_child(&child_ctx, getpid());
				if (state->options & MRSH_OPT_MONITOR) {
					init_job_child_process(state);
				}

				if (!(state->options & MRSH_OPT_MONITOR)) {
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

				int ret = run_and_or_list(&child_ctx, list->and_or_list);
				if (ret < 0) {
					exit(127);
				}
				exit(ret);
			}

			ret = 0;
			init_async_child(&child_ctx, pid);
		} else {
			ret = run_and_or_list(ctx, list->and_or_list);
			if (ret < 0) {
				return ret;
			}
		}

		if (ret >= 0) {
			state->last_status = ret;
		}
	}
	return ret;
}

static void show_job(struct mrsh_job *job, struct mrsh_job *current,
		struct mrsh_job *previous, bool r) {
	char curprev = ' ';
	if (job == current) {
		curprev = '+';
	} else if (job == previous) {
		curprev = '-';
	}
	char *cmd = mrsh_node_format(job->node);
	fprintf(stderr, "[%d] %c %s %s\n", job->job_id, curprev,
		job_state_str(job, r), cmd);
	free(cmd);
}

static void destroy_terminated_jobs(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	struct mrsh_job *current = job_by_id(state, "%+", false),
		*previous = job_by_id(state, "%-", false);
	bool r = rand() % 2 == 0;

	for (size_t i = 0; i < priv->jobs.len; ++i) {
		struct mrsh_job *job = priv->jobs.data[i];

		int status = job_poll(job);

		if (state->options & MRSH_OPT_NOTIFY && job->pending_notification) {
			show_job(job, current, previous, r);
			job->pending_notification = false;
		}

		if (status >= 0) {
			job_destroy(job);
			--i;
		}
	}

	fflush(stderr);
}

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	struct mrsh_context ctx = { .state = state };
	int ret = run_command_list_array(&ctx, &prog->body);
	destroy_terminated_jobs(state);
	return ret;
}

int mrsh_run_word(struct mrsh_state *state, struct mrsh_word **word) {
	expand_tilde(state, word, false);

	struct mrsh_context ctx = { .state = state };
	int last_status = state->last_status;
	int ret = run_word(&ctx, word);
	state->last_status = last_status;
	return ret;
}
