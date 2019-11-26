#ifndef SHELL_TASK_H
#define SHELL_TASK_H

#include "shell/shell.h"
#include "shell/word.h"

/**
 * The task is waiting for child processes to finish.
 */
#define TASK_STATUS_WAIT -1
/**
 * A fatal error occured, the task should be destroyed.
 */
#define TASK_STATUS_ERROR -2
/**
 * The task has been stopped and the job has been put in the background.
 */
#define TASK_STATUS_STOPPED -3
/**
 * The task has been interrupted for some reason.
 */
#define TASK_STATUS_INTERRUPTED -4

struct mrsh_context;

/* Perform parameter expansion, command substitution and arithmetic expansion. */
int run_word(struct mrsh_context *ctx, struct mrsh_word **word_ptr);
/* Perform all word expansions, as specified in section 2.6. Fills `fields`
 * with `char *` elements. Not suitable for assignments. */
int expand_word(struct mrsh_context *ctx, const struct mrsh_word *word,
	struct mrsh_array *fields);
int run_simple_command(struct mrsh_context *ctx, struct mrsh_simple_command *sc);
int run_command(struct mrsh_context *ctx, struct mrsh_command *cmd);
int run_and_or_list(struct mrsh_context *ctx, struct mrsh_and_or_list *and_or_list);
int run_pipeline(struct mrsh_context *ctx, struct mrsh_pipeline *pipeline);
int run_command_list_array(struct mrsh_context *ctx, struct mrsh_array *array);

#endif
