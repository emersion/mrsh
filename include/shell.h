#include <mrsh/shell.h>

struct context {
	struct mrsh_state *state;
	int stdin_fileno;
	int stdout_fileno;
};

/**
 * This struct is used to track child processes.
 */
struct process {
	pid_t pid;
	bool finished;
	int stat;
};

#define TASK_STATUS_WAIT -1
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
	 * returned state is cached and `poll` won't be called anymore..
	 */
	int (*poll)(struct task *task, struct context *ctx);
	void (*destroy)(struct task *task);
};

enum tilde_expansion {
	// Don't perform tilde expansion at all
	TILDE_EXPANSION_NONE,
	// Only expand at the begining of words
	TILDE_EXPANSION_NAME,
	// Expand at the begining of words and after semicolons
	TILDE_EXPANSION_ASSIGNMENT,
};

int create_anonymous_file(void);

void process_init(struct process *process, pid_t pid);
void process_finish(struct process *process);
int process_poll(struct process *process);
void process_notify(pid_t pid, int stat);

void task_init(struct task *task, const struct task_interface *impl);
void task_destroy(struct task *task);
int task_poll(struct task *task, struct context *ctx);
int task_run(struct task *task, struct context *ctx);

struct task *task_command_create(struct mrsh_simple_command *sc);

struct task *task_list_create(void);
void task_list_add(struct task *task, struct task *child);

struct task *task_if_clause_create(struct task *condition, struct task *body,
	struct task *else_part);

struct task *task_pipeline_create(void);
void task_pipeline_add(struct task *task, struct task *child);

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

/**
 * Performs tilde expansion. It leaves the string as-is in case of error.
 */
void expand_tilde(struct mrsh_state *state, char **str_ptr);
/**
 * Performs field splitting on `word`, writing fields to `fields`. This should
 * be done after expansions/substitutions.
 */
void split_fields(struct mrsh_array *fields, struct mrsh_word *word,
	const char *ifs);
/**
 * Performs pathname expansion on each item in `fields`.
 */
bool expand_pathnames(struct mrsh_array *expanded, struct mrsh_array *fields);
