#ifndef _MRSH_MRSH_H
#define _MRSH_MRSH_H

#include "array.h"

struct mrsh_command {
	char *name;
};

struct mrsh_pipeline {
	struct mrsh_array commands;
};

struct mrsh_complete_command {
	// TODO: missing one level
	struct mrsh_array pipelines; // mrsh_pipeline
};

struct mrsh_program {
	struct mrsh_array commands; // mrsh_complete_command
};

#endif
