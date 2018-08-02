#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

char *buffer_add(struct buffer *buf, size_t size) {
	size_t new_len = buf->len + size;
	if (new_len > buf->cap) {
		size_t new_cap = 2 * buf->cap;
		if (new_cap == 0) {
			new_cap = 32;
		}
		if (new_cap < new_len) {
			new_cap = new_len;
		}
		char *new_buf = realloc(buf->data, new_cap);
		if (new_buf == NULL) {
			return NULL;
		}

		buf->data = new_buf;
		buf->cap = new_cap;
	}

	void *data = &buf->data[buf->len];
	buf->len = new_len;
	return data;
}

bool buffer_append(struct buffer *buf, char *data, size_t size) {
	char *dst = buffer_add(buf, size);
	if (dst == NULL) {
		return false;
	}

	memcpy(dst, data, size);
	return true;
}

bool buffer_append_char(struct buffer *buf, char c) {
	char *dst = buffer_add(buf, sizeof(char));
	if (dst == NULL) {
		return false;
	}

	*dst = c;
	return true;
}

char *buffer_steal(struct buffer *buf) {
	char *data = buf->data;
	buf->data = NULL;
	buf->cap = buf->len = 0;
	return data;
}

void buffer_finish(struct buffer *buf) {
	free(buf->data);
	buf->cap = buf->len = 0;
}
