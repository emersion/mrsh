#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/task.h"

/**
 * Put the process into its job's process group. This has to be done both in the
 * parent and the child because of potential race conditions.
 */
static struct mrsh_process *init_child(struct mrsh_context *ctx, pid_t pid) {
	struct mrsh_process *proc = process_create(ctx->state, pid);
	if (ctx->state->options & MRSH_OPT_MONITOR) {
		job_add_process(ctx->job, proc);

		if (ctx->state->interactive && !ctx->background) {
			job_set_foreground(ctx->job, true, false);
		}
	}
	return proc;
}

int run_pipeline(struct mrsh_context *ctx, struct mrsh_pipeline *pl) {
	struct mrsh_state_priv *priv = state_get_priv(ctx->state);

	// Create a new sub-context, because we want one job per pipeline.
	struct mrsh_context child_ctx = *ctx;
	if (child_ctx.job == NULL) {
		child_ctx.job = job_create(ctx->state, &pl->and_or_list.node);
	}

	assert(pl->commands.len > 0);
	if (pl->commands.len == 1) {
		int ret = run_command(&child_ctx, pl->commands.data[0]);
		if (pl->bang && ret >= 0) {
			ret = !ret;
		}
		return ret;
	}

	struct mrsh_array procs = {0};
	mrsh_array_reserve(&procs, pl->commands.len);
	int next_stdin = -1, cur_stdin = -1, cur_stdout = -1;
	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];

		if (i < pl->commands.len - 1) {
			int fds[2];
			if (pipe(fds) != 0) {
				perror("pipe");
				return TASK_STATUS_ERROR;
			}

			// We'll use the write end of the pipe as stdout, the read end will
			// be used as stdin by the next command
			assert(next_stdin == -1 && cur_stdout == -1);
			next_stdin = fds[0];
			cur_stdout = fds[1];
		}

		pid_t pid = fork();
		if (pid < 0) {
			return TASK_STATUS_ERROR;
		} else if (pid == 0) {
			priv->child = true;

			init_child(&child_ctx, getpid());
			if (ctx->state->options & MRSH_OPT_MONITOR) {
				init_job_child_process(ctx->state);
			}

			if (next_stdin >= 0) {
				close(next_stdin);
			}

			if (i > 0) {
				if (dup2(cur_stdin, STDIN_FILENO) < 0) {
					fprintf(stderr, "failed to duplicate stdin: %s\n",
						strerror(errno));
					return false;
				}
				close(cur_stdin);
			}

			if (i < pl->commands.len - 1) {
				if (dup2(cur_stdout, STDOUT_FILENO) < 0) {
					fprintf(stderr, "failed to duplicate stdout: %s\n",
						strerror(errno));
					return false;
				}
				close(cur_stdout);
			}

			int ret = run_command(&child_ctx, cmd);
			if (ret < 0) {
				exit(127);
			}

			exit(ret);
		}

		struct mrsh_process *proc = init_child(&child_ctx, pid);
		mrsh_array_add(&procs, proc);

		if (cur_stdin >= 0) {
			close(cur_stdin);
			cur_stdin = -1;
		}
		if (cur_stdout >= 0) {
			close(cur_stdout);
			cur_stdout = -1;
		}

		cur_stdin = next_stdin;
		next_stdin = -1;
	}

	assert(next_stdin == -1 && cur_stdout == -1 && cur_stdin == -1);

	int ret = 0;
	for (size_t i = 0; i < procs.len; ++i) {
		struct mrsh_process *proc = procs.data[i];
		ret = job_wait_process(proc);
		if (ret < 0) {
			break;
		}
	}
	mrsh_array_finish(&procs);
	if (pl->bang && ret >= 0) {
		ret = !ret;
	}
	return ret;
}
