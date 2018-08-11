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

struct task_word {
	struct task task;
	struct mrsh_word **word_ptr;
	enum tilde_expansion tilde_expansion;

	// only if it's a command
	bool started;
	struct process process;
};

static void task_word_swap(struct task_word *tt,
		struct mrsh_word *new_word) {
	mrsh_word_destroy(*tt->word_ptr);
	*tt->word_ptr = new_word;
}

static bool task_word_command_start(struct task_word *tt,
		struct context *ctx) {
	struct mrsh_word *word = *tt->word_ptr;
	struct mrsh_word_command *wc = mrsh_word_get_command(word);
	assert(wc != NULL);

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

		FILE *f = fmemopen(wc->command, strlen(wc->command), "r");
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

		// Trim newlines at the end
		ssize_t i = buf.len - 1;
		while (i >= 0 && buf.data[i] == '\n') {
			buf.data[i] = '\0';
			--i;
		}

		buffer_append_char(&buf, '\0');
		char *str = buffer_steal(&buf);
		buffer_finish(&buf);

		struct mrsh_word_string *ws = mrsh_word_string_create(str, false);
		task_word_swap(tt, &ws->word);
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

static int task_word_poll(struct task *task, struct context *ctx) {
	struct task_word *tt = (struct task_word *)task;
	struct mrsh_word *word = *tt->word_ptr;

	switch (word->type) {
	case MRSH_WORD_STRING:;
		struct mrsh_word_string *ws = mrsh_word_get_string(word);
		assert(ws != NULL);
		if (!ws->single_quoted && tt->tilde_expansion != TILDE_EXPANSION_NONE) {
			// TODO: TILDE_EXPANSION_ASSIGNMENT
			expand_tilde(ctx->state, &ws->str);
		}
		return 0;
	case MRSH_WORD_PARAMETER:;
		struct mrsh_word_parameter *wp = mrsh_word_get_parameter(word);
		assert(wp != NULL);
		const char *value = parameter_get_value(ctx->state, wp->name);
		if (value == NULL) {
			if ((ctx->state->options & MRSH_OPT_NOUNSET)) {
				fprintf(stderr, "%s: %s: unbound variable\n",
						ctx->state->argv[0], wp->name);
				return TASK_STATUS_ERROR;
			}
			value = "";
		}
		struct mrsh_word_string *replacement =
			mrsh_word_string_create(strdup(value), false);
		task_word_swap(tt, &replacement->word);
		return 0;
	case MRSH_WORD_COMMAND:;
		struct mrsh_word_command *wc = mrsh_word_get_command(word);
		assert(wc != NULL);

		if (!tt->started) {
			if (!task_word_command_start(tt, ctx)) {
				return TASK_STATUS_ERROR;
			}
			tt->started = true;
		}

		return process_poll(&tt->process);
	case MRSH_WORD_LIST:
		assert(0);
	}
	assert(0);
}

static const struct task_interface task_word_impl = {
	.poll = task_word_poll,
};

struct task *task_word_create(struct mrsh_word **word_ptr,
		enum tilde_expansion tilde_expansion) {
	struct mrsh_word *word = *word_ptr;

	if (word->type == MRSH_WORD_LIST) {
		struct mrsh_word_list *wl = mrsh_word_get_list(word);
		assert(wl != NULL);
		struct task *task_list = task_list_create();
		for (size_t i = 0; i < wl->children.len; ++i) {
			struct mrsh_word **child_ptr =
				(struct mrsh_word **)&wl->children.data[i];
			if (i > 0 || wl->double_quoted) {
				tilde_expansion = TILDE_EXPANSION_NONE;
			}
			task_list_add(task_list,
				task_word_create(child_ptr, tilde_expansion));
		}
		return task_list;
	}

	struct task_word *tt = calloc(1, sizeof(struct task_word));
	task_init(&tt->task, &task_word_impl);
	tt->word_ptr = word_ptr;
	tt->tilde_expansion = tilde_expansion;
	return &tt->task;
}
