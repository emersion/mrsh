#ifndef SHELL_SHM_H
#define SHELL_SHM_H

#include <stdbool.h>

int create_anonymous_file(void);
bool set_cloexec(int fd);

#endif
