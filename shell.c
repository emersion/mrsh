#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <mrsh/builtin.h>
#include <mrsh/shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shell.h"

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
}

static int parse_fd(const char *str) {
	char *endptr;
	errno = 0;
	int fd = strtol(str, &endptr, 10);
	if (errno != 0) {
		return -1;
	}
	if (endptr[0] != '\0') {
		errno = EINVAL;
		return -1;
	}

	return fd;
}

static int run_simple_command(struct mrsh_state *state,
		struct mrsh_simple_command *sc, struct context *ctx) {
	int argc = 1 + sc->arguments.len;
	char *argv[argc + 1];
	argv[0] = sc->name;
	memcpy(argv + 1, sc->arguments.data, sc->arguments.len * sizeof(void *));
	argv[argc] = NULL;

	// TODO: redirections for builtins
	int ret = mrsh_builtin(state, argc, argv);
	if (ret != -1) {
		return ret;
	}

	pid_t pid = fork();
	if (pid < 0) {
		return -1;
	} else if (pid == 0) {
		for (size_t i = 0; i < sc->assignments.len; ++i) {
			struct mrsh_assignment *assign = sc->assignments.data[i];
			setenv(assign->name, assign->value, true);
		}

		if (ctx->stdin_fileno >= 0) {
			dup2(ctx->stdin_fileno, STDIN_FILENO);
		}
		if (ctx->stdout_fileno >= 0) {
			dup2(ctx->stdout_fileno, STDOUT_FILENO);
		}

		for (size_t i = 0; i < sc->io_redirects.len; ++i) {
			struct mrsh_io_redirect *redir = sc->io_redirects.data[i];

			// TODO: filename expansions
			int fd, default_redir_fd;
			errno = 0;
			if (strcmp(redir->op, "<") == 0) {
				fd = open(redir->filename, O_RDONLY);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, ">") == 0 ||
					strcmp(redir->op, ">|") == 0) {
				fd = open(redir->filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				default_redir_fd = STDOUT_FILENO;
			} else if (strcmp(redir->op, ">>") == 0) {
				fd = open(redir->filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
				default_redir_fd = STDOUT_FILENO;
			} else if (strcmp(redir->op, "<>") == 0) {
				fd = open(redir->filename, O_RDWR | O_CREAT, 0644);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, "<&") == 0) {
				// TODO: parse "-"
				fd = parse_fd(redir->filename);
				default_redir_fd = STDIN_FILENO;
			} else if (strcmp(redir->op, ">&") == 0) {
				// TODO: parse "-"
				fd = parse_fd(redir->filename);
				default_redir_fd = STDOUT_FILENO;
			} else {
				assert(false);
			}
			if (fd < 0) {
				fprintf(stderr, "cannot open %s: %s\n", redir->filename,
					strerror(errno));
				exit(EXIT_FAILURE);
			}

			int redir_fd = redir->io_number;
			if (redir_fd < 0) {
				redir_fd = default_redir_fd;
			}

			errno = 0;
			int ret = dup2(fd, redir_fd);
			if (ret < 0) {
				fprintf(stderr, "cannot duplicate file descriptor: %s\n",
					strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		errno = 0;
		execvp(sc->name, argv);

		// Something went wrong
		fprintf(stderr, "%s: %s\n", sc->name, strerror(errno));
		exit(127);
	} else {
		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		for (size_t i = 0; i < CONTEXT_PIDS_CAP; ++i) {
			if (ctx->pids[i] == 0) {
				ctx->pids[i] = pid;
				break;
			}
		}

		if (ctx->nohang) {
			return 0;
		}

		int status;
		if (waitpid(pid, &status, 0) != pid) {
			return -1;
		}
		return WEXITSTATUS(status);
	}
}

static int run_node(struct mrsh_state *state, struct mrsh_node *node,
	struct context *ctx);

static int run_command_list_array(struct mrsh_state *state,
		struct mrsh_array *array, struct context *ctx) {
	int exit_status = 0;
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		// TODO: handle list->ampersand
		exit_status = run_node(state, list->node, ctx);
	}
	return exit_status;
}

static int run_command(struct mrsh_state *state, struct mrsh_command *cmd,
	struct context *ctx);

static int run_if_clause(struct mrsh_state *state, struct mrsh_if_clause *ic,
		struct context *ctx) {
	assert(!ctx->nohang); // TODO

	int condition_status = run_command_list_array(state, &ic->condition, ctx);
	if (condition_status < 0) {
		return condition_status;
	}

	if (condition_status == 0) {
		return run_command_list_array(state, &ic->body, ctx);
	} else {
		if (ic->else_part == NULL) {
			return 0;
		}
		return run_command(state, ic->else_part, ctx);
	}
}

static int run_command(struct mrsh_state *state, struct mrsh_command *cmd,
		struct context *ctx) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		assert(sc != NULL);
		return run_simple_command(state, sc, ctx);
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		assert(bg != NULL);
		return run_command_list_array(state, &bg->body, ctx);
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		assert(ic != NULL);
		return run_if_clause(state, ic, ctx);
	}
	assert(false);
}

static int run_pipeline(struct mrsh_state *state, struct mrsh_pipeline *pl,
		struct context *parent_ctx) {
	assert(!parent_ctx->nohang); // TODO

	struct context ctx = { .nohang = true };

	int exit_status = 0;
	int last_stdout = -1;
	for (size_t i = 0; i < pl->commands.len; ++i) {
		ctx.stdin_fileno = -1;
		ctx.stdout_fileno = -1;

		if (i > 0) {
			ctx.stdin_fileno = last_stdout;
		} else {
			ctx.stdin_fileno = parent_ctx->stdin_fileno;
		}

		if (i < pl->commands.len - 1) {
			int fds[2];
			pipe(fds);
			ctx.stdout_fileno = fds[1];
			last_stdout = fds[0];
		} else {
			ctx.stdout_fileno = parent_ctx->stdout_fileno;
		}

		struct mrsh_command *cmd = pl->commands.data[i];
		int ret = run_command(state, cmd, &ctx);
		if (ret < 0) {
			return ret;
		}
	}

	for (size_t i = 0; i < CONTEXT_PIDS_CAP; ++i) {
		pid_t pid = ctx.pids[i];
		if (pid == 0) {
			break;
		}

		int status;
		if (waitpid(pid, &status, 0) != pid) {
			return -1;
		}
		exit_status = WEXITSTATUS(status);
	}

	if (pl->bang) {
		exit_status = !exit_status;
	}
	return exit_status;
}

static int run_binop(struct mrsh_state *state, struct mrsh_binop *binop,
		struct context *ctx) {
	assert(!ctx->nohang); // TODO

	int left_status = run_node(state, binop->left, ctx);
	if (left_status < 0) {
		return left_status;
	}

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

	return run_node(state, binop->right, ctx);
}

static int run_node(struct mrsh_state *state, struct mrsh_node *node,
		struct context *ctx) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl != NULL);
		return run_pipeline(state, pl, ctx);
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		assert(binop != NULL);
		return run_binop(state, binop, ctx);
	}
	assert(false);
}

int mrsh_run_command_list(struct mrsh_state *state,
		struct mrsh_command_list *list) {
	struct context ctx = {
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	return run_node(state, list->node, &ctx);
}

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	struct context ctx = {
		.stdin_fileno = -1,
		.stdout_fileno = -1,
	};
	return run_command_list_array(state, &prog->body, &ctx);
}
