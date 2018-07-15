#include <mrsh/shell.h>

#define CONTEXT_PIDS_CAP 64

struct context {
	int stdin_fileno;
	int stdout_fileno;
	bool nohang;
	pid_t pids[CONTEXT_PIDS_CAP]; // TODO: make this dynamic
};
