#ifndef SHELL_JOB_H
#define SHELL_JOB_H

#include <stdbool.h>

struct mrsh_state;

bool job_init_process(struct mrsh_state *state);

#endif
