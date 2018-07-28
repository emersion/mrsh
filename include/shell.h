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
	int (*poll)(struct task *task, struct context *ctx);
	void (*destroy)(struct task *task);
};

#define TASK_STATUS_WAIT -1
#define TASK_STATUS_ERROR -2

struct task {
	const struct task_interface *impl;
	int status;
};

void process_init(struct process *process, pid_t pid);
void process_finish(struct process *process);
int process_poll(struct process *process);
void process_notify(pid_t pid, int stat);

void task_init(struct task *task, const struct task_interface *impl);
void task_destroy(struct task *task);
int task_poll(struct task *task, struct context *ctx);
int task_run(struct task *task, struct context *ctx);

struct task *task_builtin_create(struct mrsh_simple_command *sc);

struct task *task_process_create(struct mrsh_simple_command *sc);

struct task *task_list_create(void);
void task_list_add(struct task *task, struct task *child);

struct task *task_if_clause_create(struct task *condition, struct task *body,
	struct task *else_part);

struct task *task_pipeline_create(void);
void task_pipeline_add(struct task *task, struct task *child);

struct task *task_binop_create(enum mrsh_binop_type type,
	struct task *left, struct task *right);
