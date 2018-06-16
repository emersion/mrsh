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
		errno = 0;
		execvp(sc->name, argv);
		fprintf(stderr, "%s: %s\n", sc->name, strerror(errno));
		exit(127);
	} else {
		int status;
		if (waitpid(pid, &status, 0) != pid) {
			return -1;
		}
		return status;
	}
}

static int run_command(struct mrsh_state *state, struct mrsh_command *cmd) {
	switch (cmd->type) {
	case MRSH_SIMPLE_COMMAND:;
		struct mrsh_simple_command *sc = mrsh_command_get_simple_command(cmd);
		assert(sc != NULL);
		return run_simple_command(state, sc);
	default:
		assert(false); // TODO
	}
}

static void run_pipeline(struct mrsh_state *state, struct mrsh_pipeline *pl) {
	for (size_t i = 0; i < pl->commands.len; ++i) {
		struct mrsh_command *cmd = pl->commands.data[i];
		run_command(state, cmd);
	}
}

static void run_node(struct mrsh_state *state, struct mrsh_node *node) {
	switch (node->type) {
	case MRSH_NODE_PIPELINE:;
		struct mrsh_pipeline *pl = mrsh_node_get_pipeline(node);
		assert(pl != NULL);
		run_pipeline(state, pl);
		break;
	default:
		assert(false); // TODO
	}
}

void mrsh_run_command_list(struct mrsh_state *state,
		struct mrsh_command_list *list) {
	run_node(state, list->node);
}

void mrsh_run_program(struct mrsh_state *state, struct mrsh_program *prog) {
	for (size_t i = 0; i < prog->body.len; ++i) {
		struct mrsh_command_list *list = prog->body.data[i];
		run_node(state, list->node);
	}
}
