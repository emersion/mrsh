#ifndef MRSH_ARRAY_H
#define MRSH_ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

struct mrsh_array {
	void **data;
	size_t len, cap;
};

bool mrsh_array_reserve(struct mrsh_array *array, size_t size);
ssize_t mrsh_array_add(struct mrsh_array *array, void *value);
void mrsh_array_finish(struct mrsh_array *array);

#endif
