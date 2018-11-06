#include <mrsh/buffer.h>
#include <stdlib.h>
#include <string.h>

char *mrsh_buffer_reserve(struct mrsh_buffer *buf, size_t size) {
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

	return &buf->data[buf->len];
}

char *mrsh_buffer_add(struct mrsh_buffer *buf, size_t size) {
	char *data = mrsh_buffer_reserve(buf, size);
	if (data == NULL) {
		return NULL;
	}

	buf->len += size;
	return data;
}

bool mrsh_buffer_append(struct mrsh_buffer *buf, const char *data, size_t size) {
	char *dst = mrsh_buffer_add(buf, size);
	if (dst == NULL) {
		return false;
	}

	memcpy(dst, data, size);
	return true;
}

bool mrsh_buffer_append_char(struct mrsh_buffer *buf, char c) {
	char *dst = mrsh_buffer_add(buf, sizeof(char));
	if (dst == NULL) {
		return false;
	}

	*dst = c;
	return true;
}

char *mrsh_buffer_steal(struct mrsh_buffer *buf) {
	char *data = buf->data;
	buf->data = NULL;
	buf->cap = buf->len = 0;
	return data;
}

void mrsh_buffer_finish(struct mrsh_buffer *buf) {
	free(buf->data);
	buf->data = NULL;
	buf->cap = buf->len = 0;
}
