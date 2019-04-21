#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/path.h"
#include "shell/redir.h"
#include "shell/task_command.h"

static void populate_env_iterator(const char *key, void *_var, void *_) {
	struct mrsh_variable *var = _var;
	if ((var->attribs & MRSH_VAR_ATTRIB_EXPORT)) {
		setenv(key, var->value, 1);
	}
}

/**
 * Put the process into its job's process group. This has to be done both in the
 * parent and the child because of potential race conditions.
 */
static struct mrsh_job *put_into_process_group(struct context *ctx, pid_t pid) {
	if (ctx->job == NULL) {
		ctx->job = job_create(ctx->state, pid);
	}
	setpgid(pid, ctx->job->pgid);
	return ctx->job;
}

static bool task_process_start(struct task_command *tc, struct context *ctx) {
	struct mrsh_simple_command *sc = tc->sc;
	char **argv = (char **)tc->args.data;
	const char *path = expand_path(ctx->state, argv[0], true);
	if (!path) {
		fprintf(stderr, "%s: not found\n", argv[0]);
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		if (ctx->state->options & MRSH_OPT_MONITOR) {
			struct mrsh_job *job = put_into_process_group(ctx, getpid());
			if (ctx->state->interactive && !ctx->background) {
				job_set_foreground(job, true, false);
			}
			init_job_child_process(ctx->state);
		}

		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			uint32_t prev_attribs;
			if (mrsh_env_get(ctx->state, assign->name, &prev_attribs)
					&& (prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
				fprintf(stderr, "cannot modify readonly variable %s\n",
						assign->name);
				return false;
			}
			char *value = mrsh_word_str(assign->value);
			setenv(assign->name, value, true);
			free(value);
		}

		mrsh_hashtable_for_each(&ctx->state->variables,
				populate_env_iterator, NULL);

		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];

			int redir_fd;
			int fd = process_redir(redir, &redir_fd);
			if (fd < 0) {
				exit(1);
			}

			if (fd == redir_fd) {
				continue;
			}

			int ret = dup2(fd, redir_fd);
			if (ret < 0) {
				fprintf(stderr, "cannot duplicate file descriptor: %s\n",
					strerror(errno));
				exit(1);
			}
			close(fd);
		}

		execv(path, argv);

		// Something went wrong
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		exit(127);
	}

	tc->process = process_create(ctx->state, pid);

	if (ctx->state->options & MRSH_OPT_MONITOR) {
		struct mrsh_job *job = put_into_process_group(ctx, pid);
		if (ctx->state->interactive && !ctx->background) {
			job_set_foreground(job, true, false);
		}

		job_add_process(job, tc->process);
	}

	return true;
}

int task_process_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;

	if (!tc->started) {
		if (!task_process_start(tc, ctx)) {
			return TASK_STATUS_ERROR;
		}
		tc->started = true;
	}

	return process_poll(tc->process);
}
