#include <assert.h>
#include <mrsh/array.h>
#include <stdlib.h>

#define INITIAL_SIZE 8

ssize_t mrsh_array_add(struct mrsh_array *array, void *value) {
	assert(array->len <= array->cap);

	if (array->len == array->cap) {
		size_t new_cap = 2 * array->cap;
		if (new_cap < INITIAL_SIZE) {
			new_cap = INITIAL_SIZE;
		}
		void *new_data = realloc(array->data, new_cap * sizeof(void *));
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
