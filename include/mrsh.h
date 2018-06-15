#ifndef _MRSH_MRSH_H
#define _MRSH_MRSH_H

#include "array.h"

struct mrsh_io_redirect {
	int io_number;
	char *op;
	char *filename;
};

struct mrsh_command {
	char *name;
	struct mrsh_array arguments; // char *
	struct mrsh_array io_redirects; // struct mrsh_io_redirect *
	struct mrsh_array assignments; // char *
};

struct mrsh_pipeline {
	struct mrsh_array commands; // struct mrsh_command *
};

struct mrsh_complete_command {
	// TODO: missing one level
	struct mrsh_array pipelines; // struct mrsh_pipeline *
};

struct mrsh_program {
	struct mrsh_array commands; // struct mrsh_complete_command *
};

#endif
