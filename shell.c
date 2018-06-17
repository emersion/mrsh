#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <errno.h>
#include <mrsh/shell.h>
#include <mrsh/builtin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void mrsh_state_init(struct mrsh_state *state) {
	state->exit = -1;
}

static int run_simple_command(struct mrsh_state *state,
		struct mrsh_simple_command *sc) {
	int argc = 1 + sc->arguments.len;
	char *argv[argc + 1];
	argv[0] = sc->name;
	memcpy(argv + 1, sc->arguments.data, sc->arguments.len * sizeof(void *));
	argv[argc] = NULL;

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

		errno = 0;
		execvp(sc->name, argv);

		// Something went wrong
		fprintf(stderr, "%s: %s\n", sc->name, strerror(errno));
		exit(127);
	} else {
		int status;
		if (waitpid(pid, &status, 0) != pid) {
			return -1;
		}
		return WEXITSTATUS(status);
	}
}

static int run_node(struct mrsh_state *state, struct mrsh_node *node);

static int run_command_list_array(struct mrsh_state *state,
		struct mrsh_array *array) {
	int exit_status = 0;
	for (size_t i = 0; i < array->len; ++i) {
		struct mrsh_command_list *list = array->data[i];
		// TODO: handle list->ampersand
		exit_status = run_node(state, list->node);
	}
	return exit_status;
}

static int run_command(struct mrsh_state *state, struct mrsh_command *cmd);

static int run_if_clause(struct mrsh_state *state, struct mrsh_if_clause *ic) {
	int condition_status = run_command_list_array(state, &ic->condition);
	if (condition_status < 0) {
		return condition_status;
	}

	if (condition_status == 0) {
		return run_command_list_array(state, &ic->body);
	} else {
		if (ic->else_part == NULL) {
			return 0;
		}
		return run_command(state, ic->else_part);
	}
}

static int run_command(struct mrsh_state *state, struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		assert(sc != NULL);
		return run_simple_command(state, sc);
	case MRSH_BRACE_GROUP:;
		struct mrsh_brace_group *bg = mrsh_command_get_brace_group(cmd);
		assert(bg != NULL);
		return run_command_list_array(state, &bg->body);
	case MRSH_IF_CLAUSE:;
		struct mrsh_if_clause *ic = mrsh_command_get_if_clause(cmd);
		assert(ic != NULL);
		return run_if_clause(state, ic);
	}
	assert(false);
}

static int run_pipeline(struct mrsh_state *state, struct mrsh_pipeline *pl) {
	int exit_status = 0;
	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		exit_status = run_command(state, cmd);
	}

	if (pl->bang) {
		exit_status = !exit_status;
	}
	return exit_status;
}

static int run_binop(struct mrsh_state *state, struct mrsh_binop *binop) {
	int left_status = run_node(state, binop->left);
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

	return run_node(state, binop->right);
}

static int run_node(struct mrsh_state *state, struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl != NULL);
		return run_pipeline(state, pl);
		break;
	case MRSH_NODE_BINOP:;
		struct mrsh_binop *binop = mrsh_node_get_binop(node);
		assert(binop != NULL);
		return run_binop(state, binop);
		break;
	}
	assert(false);
}

int mrsh_run_command_list(struct mrsh_state *state,
		struct mrsh_command_list *list) {
	return run_node(state, list->node);
}

int mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	return run_command_list_array(state, &prog->body);
}
