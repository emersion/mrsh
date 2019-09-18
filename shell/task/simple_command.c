#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mrsh/ast.h>
#include <mrsh/builtin.h>
#include <mrsh/entry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/shell.h"
#include "shell/path.h"
#include "shell/redir.h"
#include "shell/word.h"
#include "shell/task.h"

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
static struct process *init_child(struct context *ctx, pid_t pid) {
	struct process *proc = process_create(ctx->state, pid);
	if (ctx->state->options & MRSH_OPT_MONITOR) {
		job_add_process(ctx->job, proc);

		if (ctx->state->interactive && !ctx->background) {
			job_set_foreground(ctx->job, true, false);
		}
	}
	return proc;
}

static int run_process(struct context *ctx, struct mrsh_simple_command *sc,
		char **argv) {
	// The pipeline is responsible for creating the job
	assert(ctx->job != NULL);

	const char *path = expand_path(ctx->state, argv[0], true);
	if (!path) {
		fprintf(stderr, "%s: not found\n", argv[0]);
		return TASK_STATUS_ERROR;
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return TASK_STATUS_ERROR;
	} else if (pid == 0) {
		init_child(ctx, getpid());
		if (ctx->state->options & MRSH_OPT_MONITOR) {
			init_job_child_process(ctx->state);
		}

		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			uint32_t prev_attribs;
			if (mrsh_env_get(ctx->state, assign->name, &prev_attribs)
					&& (prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
				fprintf(stderr, "cannot modify readonly variable %s\n",
						assign->name);
				exit(1);
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

	struct process *process = init_child(ctx, pid);
	return job_wait_process(process);
}

struct saved_fd {
	int dup_fd;
	int redir_fd;
};

bool dup_and_save_fd(int fd, int redir_fd, struct saved_fd *saved) {
	saved->redir_fd = redir_fd;

	if (fd == redir_fd) {
		return true;
	}

	saved->dup_fd = dup(redir_fd);
	if (saved->dup_fd < 0) {
		fprintf(stderr, "failed to duplicate file descriptor: %s\n",
			strerror(errno));
		return false;
	}

	if (dup2(fd, redir_fd) < 0) {
		fprintf(stderr, "failed to duplicate file descriptor: %s\n",
			strerror(errno));
		return false;
	}
	close(fd);

	return true;
}

static int run_builtin(struct context *ctx, struct mrsh_simple_command *sc,
		int argc, char **argv) {
	// Duplicate old FDs to be able to restore them later
	// Zero-length VLAs are undefined behaviour
	struct saved_fd fds[sc->io_redirects.len + 1];
	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
		fds[i].dup_fd = fds[i].redir_fd = -1;
	}

	for (size_t i = 0; i < sc->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
		struct saved_fd *saved = &fds[i];

		int redir_fd;
		int fd = process_redir(redir, &redir_fd);
		if (fd < 0) {
			return TASK_STATUS_ERROR;
		}

		if (!dup_and_save_fd(fd, redir_fd, saved)) {
			return TASK_STATUS_ERROR;
		}
	}

	// TODO: environment from assignements

	int ret = mrsh_run_builtin(ctx->state, argc, argv);

	// In case stdout/stderr are pipes, we need to flush to ensure output lines
	// aren't out-of-order
	fflush(stdout);
	fflush(stderr);

	// Restore old FDs
	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
		if (fds[i].dup_fd < 0) {
			continue;
		}

		if (dup2(fds[i].dup_fd, fds[i].redir_fd) < 0) {
			fprintf(stderr, "failed to duplicate file descriptor: %s\n",
				strerror(errno));
			return TASK_STATUS_ERROR;
		}
		close(fds[i].dup_fd);
	}

	return ret;
}

static int run_assignments(struct context *ctx, struct mrsh_array *assignments) {
	for (size_t i = 0; i < assignments->len; ++i) {
		struct mrsh_assignment *assign = assignments->data[i];
		char *new_value = mrsh_word_str(assign->value);
		uint32_t attribs = MRSH_VAR_ATTRIB_NONE;
		if ((ctx->state->options & MRSH_OPT_ALLEXPORT)) {
			attribs = MRSH_VAR_ATTRIB_EXPORT;
		}
		uint32_t prev_attribs = 0;
		if (mrsh_env_get(ctx->state, assign->name, &prev_attribs) != NULL
				&& (prev_attribs & MRSH_VAR_ATTRIB_READONLY)) {
			free(new_value);
			fprintf(stderr, "cannot modify readonly variable %s\n",
				assign->name);
			return TASK_STATUS_ERROR;
		}
		mrsh_env_set(ctx->state, assign->name, new_value, attribs);
		free(new_value);
	}

	return 0;
}

static void expand_assignments(struct context *ctx,
		struct mrsh_array *assignments) {
	for (size_t i = 0; i < assignments->len; ++i) {
		struct mrsh_assignment *assign = assignments->data[i];
		run_word(ctx, &assign->value, TILDE_EXPANSION_ASSIGNMENT);
		// TODO: report errors
	}
}

static void get_args(struct mrsh_array *args, struct mrsh_simple_command *sc,
		struct context *ctx) {
	struct mrsh_array fields = {0};
	const char *ifs = mrsh_env_get(ctx->state, "IFS", NULL);
	split_fields(&fields, sc->name, ifs);
	for (size_t i = 0; i < sc->arguments.len; ++i) {
		struct mrsh_word *word = sc->arguments.data[i];
		split_fields(&fields, word, ifs);
	}
	assert(fields.len > 0);

	if (ctx->state->options & MRSH_OPT_NOGLOB) {
		get_fields_str(args, &fields);
	} else {
		expand_pathnames(args, &fields);
	}

	for (size_t i = 0; i < fields.len; ++i) {
		mrsh_word_destroy(fields.data[i]);
	}
	mrsh_array_finish(&fields);

	assert(args->len > 0);
	mrsh_array_add(args, NULL);
}

static struct mrsh_simple_command *copy_simple_command(
		const struct mrsh_simple_command *sc) {
	struct mrsh_command *cmd = mrsh_command_copy(&sc->command);
	return mrsh_command_get_simple_command(cmd);
}

int run_simple_command(struct context *ctx, struct mrsh_simple_command *sc) {
	if (sc->name == NULL) {
		// Copy each assignment from the AST, because during expansion and
		// substitution we'll mutate the tree
		struct mrsh_array assignments = {0};
		mrsh_array_reserve(&assignments, sc->assignments.len);
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			mrsh_array_add(&assignments, mrsh_assignment_copy(assign));
		}

		expand_assignments(ctx, &assignments);
		int ret = run_assignments(ctx, &assignments);

		for (size_t i = 0; i < assignments.len; ++i) {
			struct mrsh_assignment *assign = assignments.data[i];
			mrsh_assignment_destroy(assign);
		}
		mrsh_array_finish(&assignments);
		return ret;
	}

	// Copy the command from the AST, because during expansion and substitution
	// we'll mutate the tree
	sc = copy_simple_command(sc);

	int ret = run_word(ctx, &sc->name, TILDE_EXPANSION_NAME);
	if (ret < 0) {
		return ret;
	}
	expand_assignments(ctx, &sc->assignments);

	for (size_t i = 0; i < sc->arguments.len; ++i) {
		struct mrsh_word **arg_ptr =
			(struct mrsh_word **)&sc->arguments.data[i];
		ret = run_word(ctx, arg_ptr, TILDE_EXPANSION_NAME);
		if (ret < 0) {
			return ret;
		}
	}

	for (size_t i = 0; i < sc->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
		ret = run_word(ctx, &redir->name, TILDE_EXPANSION_NAME);
		if (ret < 0) {
			return ret;
		}
		for (size_t j = 0; j < redir->here_document.len; ++j) {
			struct mrsh_word **line_word_ptr =
				(struct mrsh_word **)&redir->here_document.data[j];
			ret = run_word(ctx, line_word_ptr, TILDE_EXPANSION_NAME);
			if (ret < 0) {
				return ret;
			}
		}
	}

	struct mrsh_array args = {0};
	get_args(&args, sc, ctx);

	char **argv = (char **)args.data;
	int argc = args.len - 1; // argv is NULL-terminated
	const char *argv_0 = argv[0];

	if ((ctx->state->options & MRSH_OPT_XTRACE)) {
		char *ps4 = mrsh_get_ps4(ctx->state);
		fprintf(stderr, "%s", ps4);
		for (int i = 0; i < argc; ++i) {
			fprintf(stderr, "%s%s", i > 0 ? " " : "", argv[i]);
		}
		fprintf(stderr, "\n");
		free(ps4);
	}

	ret = -1;
	const struct mrsh_function *fn_def =
		mrsh_hashtable_get(&ctx->state->functions, argv_0);
	if (fn_def != NULL) {
		mrsh_push_frame(ctx->state, argc, (const char **)argv);
		// fn_def may be free'd during run_command when overwritten with another
		// function, so we need to copy it.
		struct mrsh_command *body = mrsh_command_copy(fn_def->body);
		ret = run_command(ctx, body);
		mrsh_command_destroy(body);
		mrsh_pop_frame(ctx->state);
	} else if (mrsh_has_builtin(argv_0)) {
		ret = run_builtin(ctx, sc, argc, argv);
	} else {
		ret = run_process(ctx, sc, argv);
	}

	mrsh_command_destroy(&sc->command);
	for (size_t i = 0; i < args.len; ++i) {
		free(args.data[i]);
	}
	mrsh_array_finish(&args);
	return ret;
}
