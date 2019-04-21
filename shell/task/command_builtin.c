#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <mrsh/builtin.h>
#include "shell/redir.h"
#include "shell/task_command.h"

struct saved_fd {
	int dup_fd;
	int redir_fd;
};

bool dup_and_save_fd(int fd, int redir_fd, struct saved_fd *saved) {
	saved->redir_fd = redir_fd;

	if (fd == redir_fd) {
		return true;
	}

	saved->dup_fd = dup(redir_fd);
	if (saved->dup_fd < 0) {
		fprintf(stderr, "failed to duplicate file descriptor: %s\n",
			strerror(errno));
		return false;
	}

	if (dup2(fd, redir_fd) < 0) {
		fprintf(stderr, "failed to duplicate file descriptor: %s\n",
			strerror(errno));
		return false;
	}
	close(fd);

	return true;
}

int task_builtin_poll(struct task *task, struct context *ctx) {
	struct task_command *tc = (struct task_command *)task;
	struct mrsh_simple_command *sc = tc->sc;

	assert(!tc->started);
	tc->started = true;

	// Duplicate old FDs to be able to restore them later
	// Zero-length VLAs are undefined behaviour
	struct saved_fd fds[sc->io_redirects.len + 1];
	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
		fds[i].dup_fd = fds[i].redir_fd = -1;
	}

	for (size_t i = 0; i < sc->io_redirects.len; ++i) {
		struct mrsh_io_redirect *redir = sc->io_redirects.data[i];
		struct saved_fd *saved = &fds[2 + i];

		int redir_fd;
		int fd = process_redir(redir, &redir_fd);
		if (fd < 0) {
			return TASK_STATUS_ERROR;
		}

		if (!dup_and_save_fd(fd, redir_fd, saved)) {
			return TASK_STATUS_ERROR;
		}
	}

	// TODO: environment from assignements

	int argc = tc->args.len - 1;
	char **argv = (char **)tc->args.data;
	int ret = mrsh_run_builtin(ctx->state, argc, argv);

	// In case stdout/stderr are pipes, we need to flush to ensure output lines
	// aren't out-of-order
	fflush(stdout);
	fflush(stderr);

	// Restore old FDs
	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
		if (fds[i].dup_fd < 0) {
			continue;
		}

		if (dup2(fds[i].dup_fd, fds[i].redir_fd) < 0) {
			fprintf(stderr, "failed to duplicate file descriptor: %s\n",
				strerror(errno));
			return TASK_STATUS_ERROR;
		}
		close(fds[i].dup_fd);
	}

	return ret;
}
