#include <assert.h>
#include <stdlib.h>

#include "array.h"

#define INITIAL_SIZE 32

void mrsh_array_init(struct mrsh_array *array) {
	array->data = malloc(INITIAL_SIZE * sizeof(void *));
	array->cap = INITIAL_SIZE;
	array->len = 0;
}

ssize_t mrsh_array_add(struct mrsh_array *array, void *value) {
	assert(array->data != NULL);
	assert(array->len <= array->cap);

	if (array->len == array->cap) {
		size_t new_cap = 2 * array->cap;
		void *new_data = realloc(array->data, new_cap);
		if (new_data == NULL) {
			return -1;
		}
		array->data = new_data;
		array->cap = new_cap;
	}

	size_t i = array->len;
	array->data[i] = value;
	array->len++;
	return i;
}

void mrsh_array_finish(struct mrsh_array *array) {
	free(array->data);
	array->cap = array->len = 0;
}
