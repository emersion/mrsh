#include <mrsh/shell.h>

struct context {
	struct mrsh_state *state;
	int stdin_fileno;
	int stdout_fileno;
};

struct process {
	pid_t pid;
	bool finished;
	int stat;
};

struct task;

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

#define TASK_STATUS_WAIT -1
#define TASK_STATUS_ERROR -2

struct task {
	const struct task_interface *impl;
	int status; // last task status
};

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
 * Creates a task that mutates `token_ptr`, executing all substitutions. After
 * the task has finished, the token tree is guaranteed to only contain token
 * lists and token strings.
 */
struct task *task_token_create(struct mrsh_token **token_ptr);
