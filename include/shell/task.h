#ifndef _SHELL_TASK_H
#define _SHELL_TASK_H

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

struct task_interface;

/**
 * Tasks abstract away operations that need to be done by the shell. When the
 * shell executes a command, it walks the AST and translates it to a tree of
 * tasks to execute.
 *
 * Tasks are required for operations that are executed in parallel without
 * subshells. POSIX allows for instance nested pipelines:
 *
 *   echo abc | { cat | cat; } | cat
 *
 * In this case the shell should not block before executing the last `cat`
 * command.
 */
struct task {
	const struct task_interface *impl;
	int status; // last task status
};

struct task_interface {
	/**
	 * Request a status update from the task. This starts or continues it.
	 * `poll` must return without blocking with the current task's status:
	 *
	 * - TASK_STATUS_WAIT in case the task is pending
	 * - TASK_STATUS_ERROR in case a fatal error occured
	 * - A positive (or null) code in case the task finished
	 *
	 * `poll` will be called over and over until the task goes out of the
	 * TASK_STATUS_WAIT state. Once the task is no longer in progress, the
	 * returned state is cached and `poll` won't be called anymore.
	 */
	int (*poll)(struct task *task, struct context *ctx);
	void (*destroy)(struct task *task);
};

void task_init(struct task *task, const struct task_interface *impl);
void task_destroy(struct task *task);
int task_poll(struct task *task, struct context *ctx);
int task_run(struct task *task, struct context *ctx);

/**
 * Creates a task executing the provided simple command. The simple command will
 * be mutated during expansion and substitution.
 */
struct task *task_command_create(struct mrsh_simple_command *sc);
struct task *task_if_clause_create(struct task *condition, struct task *body,
	struct task *else_part);
struct task *task_loop_clause_create(const struct mrsh_array *condition,
	const struct mrsh_array *body, bool until);
struct task *task_for_clause_create(const char *name,
	const struct mrsh_array *word_list, const struct mrsh_array *body);
struct task *task_function_definition_create(const char *name,
	const struct mrsh_command *body);
struct task *task_binop_create(enum mrsh_binop_type type,
	struct task *left, struct task *right);
struct task *task_async_create(struct task *async);
struct task *task_assignment_create(struct mrsh_array *assignments);
/**
 * Creates a task that mutates `word_ptr`, executing all substitutions. After
 * the task has finished, the word tree is guaranteed to only contain word
 * lists and word strings.
 */
struct task *task_word_create(struct mrsh_word **word_ptr,
	enum tilde_expansion tilde_expansion);
struct task *task_subshell_create(struct task *subtask);

struct task *task_list_create(void);
void task_list_add(struct task *task, struct task *child);

struct task *task_pipeline_create(void);
void task_pipeline_add(struct task *task, struct task *child);

struct task *task_for_command_list_array(const struct mrsh_array *array);
struct task *task_for_command(const struct mrsh_command *cmd);

#endif
