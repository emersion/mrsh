#ifndef _BUFFER_H
#define _BUFFER_H

struct buffer {
	char *data;
	size_t len, cap;
};

/**
 * Makes sure at least `size` bytes can be written to the buffer, without
 * increasing its length. Returns a pointer to the end of the buffer, or NULL if
 * resizing fails.
 */
char *buffer_reserve(struct buffer *buf, size_t size);
/**
 * Increases the length of the buffer by `size` bytes. Returns a pointer to the
 * beginning of the newly appended space, or NULL if resizing fails.
 */
char *buffer_add(struct buffer *buf, size_t size);
bool buffer_append(struct buffer *buf, char *data, size_t size);
bool buffer_append_char(struct buffer *buf, char c);
/**
 * Get the buffer's current data and reset it.
 */
char *buffer_steal(struct buffer *buf);
void buffer_finish(struct buffer *buf);

#endif
