#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <mrsh/parser.h>

#include "buffer.h"
#include "shell.h"
#include "builtin.h"

#define TOKEN_COMMAND_READ_SIZE 128

struct task_token {
	struct task task;
	struct mrsh_token **token_ptr;

	// only if it's a command
	bool started;
	struct process process;
};

static void task_token_swap(struct task_token *tt,
		struct mrsh_token *new_token) {
	mrsh_token_destroy(*tt->token_ptr);
	*tt->token_ptr = new_token;
}

static bool task_token_command_start(struct task_token *tt,
		struct context *ctx) {
	struct mrsh_token *token = *tt->token_ptr;
	struct mrsh_token_command *tc = mrsh_token_get_command(token);
	assert(tc != NULL);

	int fds[2];
	if (pipe(fds) != 0) {
		fprintf(stderr, "failed to pipe(): %s\n", strerror(errno));
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "failed to fork(): %s\n", strerror(errno));
		return false;
	} else if (pid == 0) {
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);

		if (ctx->stdin_fileno >= 0) {
			close(ctx->stdin_fileno);
		}
		if (ctx->stdout_fileno >= 0) {
			close(ctx->stdout_fileno);
		}

		FILE *f = fmemopen(tc->command, strlen(tc->command), "r");
		if (f == NULL) {
			fprintf(stderr, "failed to fmemopen(): %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		struct mrsh_program *prog = mrsh_parse(f);
		fclose(f);
		if (prog == NULL) {
			exit(EXIT_SUCCESS);
		}

		struct mrsh_state state = {0};
		mrsh_state_init(&state);
		mrsh_run_program(&state, prog);
		mrsh_state_finish(&state);

		exit(state.exit >= 0 ? state.exit : EXIT_SUCCESS);
	} else {
		close(fds[1]);
		process_init(&tt->process, pid);

		// TODO: don't block here
		struct buffer buf = {0};
		while (true) {
			char *out = buffer_reserve(&buf, TOKEN_COMMAND_READ_SIZE);
			ssize_t n_read = read(fds[0], out, TOKEN_COMMAND_READ_SIZE - 1);
			if (n_read < 0) {
				buffer_finish(&buf);
				close(fds[0]);
				fprintf(stderr, "failed to read(): %s", strerror(errno));
				return false;
			} else if (n_read == 0) {
				break;
			}
			buf.len += n_read;
		}

		close(fds[0]);

		buffer_append_char(&buf, '\0');
		char *str = buffer_steal(&buf);
		buffer_finish(&buf);

		struct mrsh_token_string *ts = mrsh_token_string_create(str, false);
		task_token_swap(tt, &ts->token);
		return true;
	}
}

static const char *parameter_get_value(struct mrsh_state *state, char *name) {
	static char value[16];
	char *end;
	long lvalue = strtol(name, &end, 10);
	// Special cases
	if (strcmp(name, "@") == 0) {
		// TODO
	} else if (strcmp(name, "*") == 0) {
		// TODO
	} else if (strcmp(name, "#") == 0) {
		sprintf(value, "%d", state->argc);
		return value;
	} else if (strcmp(name, "?") == 0) {
		sprintf(value, "%d", state->last_status);
		return value;
	} else if (strcmp(name, "-") == 0) {
		return print_options(state);
	} else if (strcmp(name, "$") == 0) {
		sprintf(value, "%d", (int)getpid());
		return value;
	} else if (strcmp(name, "!") == 0) {
		// TODO
	} else if (!end[0]) {
		if (state->argc < lvalue) {
			return NULL;
		}
		return state->argv[lvalue];
	}
	// User-set cases
	return (const char *)mrsh_hashtable_get(&state->variables, name);
}

static int task_token_poll(struct task *task, struct context *ctx) {
	struct task_token *tt = (struct task_token *)task;
	struct mrsh_token *token = *tt->token_ptr;

	switch (token->type) {
	case MRSH_TOKEN_STRING:
		return 0; // Nothing to do
	case MRSH_TOKEN_PARAMETER:;
		struct mrsh_token_parameter *tp = mrsh_token_get_parameter(token);
		assert(tp != NULL);
		const char *value = parameter_get_value(ctx->state, tp->name);
		if (value == NULL) {
			value = "";
		}
		struct mrsh_token_string *ts =
			mrsh_token_string_create(strdup(value), false);
		task_token_swap(tt, &ts->token);
		return 0;
	case MRSH_TOKEN_COMMAND:;
		struct mrsh_token_command *tc = mrsh_token_get_command(token);
		assert(tc != NULL);

		if (!tt->started) {
			if (!task_token_command_start(tt, ctx)) {
				return TASK_STATUS_ERROR;
			}
			tt->started = true;
		}

		return process_poll(&tt->process);
	case MRSH_TOKEN_LIST:
		assert(0);
	}
	assert(0);
}

static const struct task_interface task_token_impl = {
	.poll = task_token_poll,
};

struct task *task_token_create(struct mrsh_token **token_ptr) {
	struct mrsh_token *token = *token_ptr;

	if (token->type == MRSH_TOKEN_LIST) {
		struct mrsh_token_list *tl = mrsh_token_get_list(token);
		assert(tl != NULL);
		struct task *task_list = task_list_create();
		for (size_t i = 0; i < tl->children.len; ++i) {
			struct mrsh_token **child_ptr =
				(struct mrsh_token **)&tl->children.data[i];
			task_list_add(task_list, task_token_create(child_ptr));
		}
		return task_list;
	}

	struct task_token *tt = calloc(1, sizeof(struct task_token));
	task_init(&tt->task, &task_token_impl);
	tt->token_ptr = token_ptr;
	return &tt->task;
}
