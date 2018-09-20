#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdbool.h>
#include <stddef.h>

struct mrsh_buffer {
	char *data;
	size_t len, cap;
};

/**
 * Makes sure at least `size` bytes can be written to the buffer, without
 * increasing its length. Returns a pointer to the end of the buffer, or NULL if
 * resizing fails.
 */
char *mrsh_buffer_reserve(struct mrsh_buffer *buf, size_t size);
/**
 * Increases the length of the buffer by `size` bytes. Returns a pointer to the
 * beginning of the newly appended space, or NULL if resizing fails.
 */
char *mrsh_buffer_add(struct mrsh_buffer *buf, size_t size);
bool mrsh_buffer_append(struct mrsh_buffer *buf, const char *data, size_t size);
bool mrsh_buffer_append_char(struct mrsh_buffer *buf, char c);
/**
 * Get the buffer's current data and reset it.
 */
char *mrsh_buffer_steal(struct mrsh_buffer *buf);
void mrsh_buffer_finish(struct mrsh_buffer *buf);

#endif
