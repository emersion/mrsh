#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

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
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}
