#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include "shell/shm.h"

static void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

int create_anonymous_file(void) {
	int retries = 100;
	do {
		char name[] = "/mrsh-XXXXXX";
		randname(name + strlen(name) - 6);

		--retries;
		// shm_open guarantees O_CLOEXEC
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

bool set_cloexec(int fd) {
	long flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		return false;
	}

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		return false;
	}

	return true;
}
