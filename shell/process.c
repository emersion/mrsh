#include <assert.h>
#include <mrsh/array.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "shell/process.h"
#include "shell/task.h"

struct mrsh_process *process_create(struct mrsh_state *state, pid_t pid) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	struct mrsh_process *proc = calloc(1, sizeof(struct mrsh_process));
	proc->pid = pid;
	proc->state = state;
	mrsh_array_add(&priv->processes, proc);
	return proc;
}

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
		(array->len - i - 1) * sizeof(void *));
	--array->len;
}

void process_destroy(struct mrsh_process *proc) {
	struct mrsh_state_priv *priv = state_get_priv(proc->state);

	for (size_t i = 0; i < priv->processes.len; ++i) {
		if (priv->processes.data[i] == proc) {
			array_remove(&priv->processes, i);
			break;
		}
	}

	free(proc);
}

int process_poll(struct mrsh_process *proc) {
	if (proc->stopped) {
		return TASK_STATUS_STOPPED;
	} else if (!proc->terminated) {
		return TASK_STATUS_WAIT;
	}

	if (WIFEXITED(proc->stat)) {
		return WEXITSTATUS(proc->stat);
	} else if (WIFSIGNALED(proc->stat)) {
		return 129; // POSIX requires >128
	} else {
		assert(false);
	}
}

void update_process(struct mrsh_state *state, pid_t pid, int stat) {
	struct mrsh_state_priv *priv = state_get_priv(state);

	struct mrsh_process *proc = NULL;
	bool found = false;
	for (size_t i = 0; i < priv->processes.len; ++i) {
		proc = priv->processes.data[i];
		if (proc->pid == pid) {
			found = true;
			break;
		}
	}
	if (!found) {
		return;
	}

	if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
		proc->terminated = true;
		proc->stat = stat;
	} else if (WIFSTOPPED(stat)) {
		proc->stopped = true;
		proc->signal = WSTOPSIG(stat);
	} else {
		assert(false);
	}
}
