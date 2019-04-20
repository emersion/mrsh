#include <assert.h>
#include <mrsh/array.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include "shell/process.h"

void process_init(struct process *proc, struct mrsh_state *state, pid_t pid) {
	mrsh_array_add(&state->processes, proc);
	proc->pid = pid;
	proc->state = state;
	proc->finished = false;
	proc->stat = 0;
}

int process_poll(struct process *proc) {
	if (!proc->finished) {
		return -1;
	}
	return WEXITSTATUS(proc->stat);
}

static void array_remove(struct mrsh_array *array, size_t i) {
	memmove(&array->data[i], &array->data[i + 1],
		(array->len - i - 1) * sizeof(void *));
	--array->len;
}

static void process_remove(struct process *proc) {
	struct mrsh_state *state = proc->state;
	for (size_t i = 0; i < state->processes.len; ++i) {
		if (state->processes.data[i] == proc) {
			array_remove(&state->processes, i);
			break;
		}
	}
}

void process_finish(struct process *proc) {
	process_remove(proc);
}

void process_notify(struct mrsh_state *state, pid_t pid, int stat) {
	struct process *proc = NULL;
	bool found = false;
	for (size_t i = 0; i < state->processes.len; ++i) {
		proc = state->processes.data[i];
		if (proc->pid == pid) {
			found = true;
			break;
		}
	}
	if (!found) {
		return;
	}

	if (WIFEXITED(stat) || WIFSIGNALED(stat)) {
		proc->finished = true;
		proc->stat = stat;
		process_remove(proc);
	} else {
		assert(false);
	}
}
