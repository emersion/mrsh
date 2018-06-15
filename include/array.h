#ifndef _MRSH_ARRAY_H
#define _MRSH_ARRAY_H

#include <stddef.h>
#include <sys/types.h>

struct mrsh_array {
	void **data;
	size_t len, cap;
};

void mrsh_array_init(struct mrsh_array *array);
ssize_t mrsh_array_add(struct mrsh_array *array, void *value);
void mrsh_array_finish(struct mrsh_array *array);

#endif
