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

struct context;

int run_word(struct context *ctx, struct mrsh_word **word_ptr,
	enum tilde_expansion tilde_expansion);
int run_simple_command(struct context *ctx, struct mrsh_simple_command *sc);
int run_command(struct context *ctx, struct mrsh_command *cmd);
int run_node(struct context *ctx, struct mrsh_node *node);
int run_command_list_array(struct context *ctx, struct mrsh_array *array);

#endif
