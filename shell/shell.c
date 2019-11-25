#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <mrsh/hashtable.h>
#include <mrsh/parser.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell/job.h"
#include "shell/shell.h"
#include "shell/process.h"

void function_destroy(struct mrsh_function *fn) {
	if (!fn) {
		return;
	}
	mrsh_command_destroy(fn->body);
	free(fn);
}

struct mrsh_state *mrsh_state_create(void) {
	struct mrsh_state_priv *priv = calloc(1, sizeof(*priv));
	if (priv == NULL) {
		return NULL;
	}

	priv->term_fd = STDIN_FILENO;

	struct mrsh_state *state = &priv->pub;
	state->exit = -1;
	state->interactive = isatty(priv->term_fd);
	state->options = state->interactive ? MRSH_OPT_INTERACTIVE : 0;

	struct mrsh_call_frame_priv *frame_priv =
		calloc(1, sizeof(struct mrsh_call_frame_priv));
	if (frame_priv == NULL) {
		free(priv);
		return NULL;
	}
	state->frame = &frame_priv->pub;

	return state;
}

static const char *get_alias_func(const char *name, void *data) {
	struct mrsh_state *state = data;
	struct mrsh_state_priv *priv = state_get_priv(state);
	return mrsh_hashtable_get(&priv->aliases, name);
}

void mrsh_state_set_parser_alias_func(
		struct mrsh_state *state, struct mrsh_parser *parser) {
	mrsh_parser_set_alias_func(parser, get_alias_func, state);
}

static void state_string_finish_iterator(const char *key, void *value,
		void *user_data) {
	free(value);
}

static void variable_destroy(struct mrsh_variable *var) {
	if (!var) {
		return;
	}
	free(var->value);
	free(var);
}

static void state_var_finish_iterator(const char *key, void *value,
		void *user_data) {
	variable_destroy((struct mrsh_variable *)value);
}

static void state_fn_finish_iterator(const char *key, void *value, void *_) {
	function_destroy((struct mrsh_function *)value);
}

static void call_frame_destroy(struct mrsh_call_frame *frame) {
	for (int i = 0; i < frame->argc; ++i) {
		free(frame->argv[i]);
	}
	free(frame->argv);
	free(frame);
}

void mrsh_state_destroy(struct mrsh_state *state) {
	struct mrsh_state_priv *priv = state_get_priv(state);
	mrsh_hashtable_for_each(&priv->variables, state_var_finish_iterator, NULL);
	mrsh_hashtable_finish(&priv->variables);
	mrsh_hashtable_for_each(&priv->functions, state_fn_finish_iterator, NULL);
	mrsh_hashtable_finish(&priv->functions);
	mrsh_hashtable_for_each(&priv->aliases,
		state_string_finish_iterator, NULL);
	mrsh_hashtable_finish(&priv->aliases);
	while (priv->jobs.len > 0) {
		job_destroy(priv->jobs.data[priv->jobs.len - 1]);
	}
	mrsh_array_finish(&priv->jobs);
	while (priv->processes.len > 0) {
		process_destroy(priv->processes.data[priv->processes.len - 1]);
	}
	mrsh_array_finish(&priv->processes);
	struct mrsh_call_frame *frame = state->frame;
	while (frame) {
		struct mrsh_call_frame *prev = frame->prev;
		call_frame_destroy(frame);
		frame = prev;
	}
	free(state);
}

struct mrsh_state_priv *state_get_priv(struct mrsh_state *state) {
	return (struct mrsh_state_priv *)state;
}

void mrsh_env_set(struct mrsh_state *state,
		const char *key, const char *value, uint32_t attribs) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	struct mrsh_variable *var = calloc(1, sizeof(struct mrsh_variable));
	if (!var) {
		return;
	}
	var->value = strdup(value);
	var->attribs = attribs;
	struct mrsh_variable *old = mrsh_hashtable_set(&priv->variables, key, var);
	variable_destroy(old);
}

void mrsh_env_unset(struct mrsh_state *state, const char *key) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	variable_destroy(mrsh_hashtable_del(&priv->variables, key));
}

const char *mrsh_env_get(struct mrsh_state *state,
		const char *key, uint32_t *attribs) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	struct mrsh_variable *var = mrsh_hashtable_get(&priv->variables, key);
	if (var && attribs) {
		*attribs = var->attribs;
	}
	return var ? var->value : NULL;
}

struct mrsh_call_frame_priv *call_frame_get_priv(struct mrsh_call_frame *frame) {
	return (struct mrsh_call_frame_priv *)frame;
}

void push_frame(struct mrsh_state *state, int argc, const char *argv[]) {
	struct mrsh_call_frame_priv *next = calloc(1, sizeof(*next));
	next->pub.argc = argc;
	next->pub.argv = malloc(sizeof(char *) * argc);
	for (int i = 0; i < argc; ++i) {
		next->pub.argv[i] = strdup(argv[i]);
	}
	next->pub.prev = state->frame;
	state->frame = &next->pub;
}

void pop_frame(struct mrsh_state *state) {
	struct mrsh_call_frame *frame = state->frame;
	assert(frame->prev != NULL);
	state->frame = frame->prev;
	call_frame_destroy(frame);
}
