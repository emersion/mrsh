#ifndef _BUFFER_H
#define _BUFFER_H

struct buffer {
	char *data;
	size_t len, cap;
};

char *buffer_add(struct buffer *buf, size_t size);
bool buffer_append(struct buffer *buf, char *data, size_t size);
bool buffer_append_char(struct buffer *buf, char c);
/**
 * Get the buffer's current data and reset it.
 */
char *buffer_steal(struct buffer *buf);
void buffer_finish(struct buffer *buf);

#endif
