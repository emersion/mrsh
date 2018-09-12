#include <mrsh/array.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include "shell/process.h"

// TODO: don't use a global
// TODO: use a linked list instead
static struct mrsh_array running_processes = {0};

void process_init(struct process *proc, pid_t pid) {
	mrsh_array_add(&running_processes, proc);
	proc->pid = pid;
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
	for (size_t i = 0; i < running_processes.len; ++i) {
		if (running_processes.data[i] == proc) {
			array_remove(&running_processes, i);
			break;
		}
	}
}

void process_finish(struct process *proc) {
	process_remove(proc);
}

void process_notify(pid_t pid, int stat) {
	for (size_t i = 0; i < running_processes.len; ++i) {
		struct process *proc = running_processes.data[i];
		if (proc->pid == pid) {
			proc->finished = true;
			proc->stat = stat;
			process_remove(proc);
			break;
		}
	}
}
