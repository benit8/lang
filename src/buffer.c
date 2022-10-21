#include <assert.h>
#include <string.h>
#include "buffer.h"

buffer_t buffer_new(size_t element_size)
{
	buffer_t buf;
	buf.data = NULL;
	buf.size = 0;
	buf.element_size = element_size;
	buf.capacity = 0;
	return buf;
}

void buffer_free(buffer_t* buf)
{
	if (buf->data)
		FREE(buf->data);
	buf->data = NULL;
	buf->size = 0;
	buf->capacity = 0;
}

bool buffer_push(buffer_t* buf, void* element)
{
	bool has_allocated = false;

	if (buf->size + 1 > buf->capacity) {
		void* new_buffer = ALLOC(buf->element_size * (buf->capacity + 16));
		assert(new_buffer);
		has_allocated = true;
		if (buf->data != NULL) {
			memmove(new_buffer, buf->data, buf->element_size * buf->capacity);
			FREE(buf->data);
		}
		buf->data = new_buffer;
		buf->capacity += 16;
	}

	memcpy((uint8_t*)buf->data + buf->element_size * buf->size, element, buf->element_size);
	buf->size++;
	return has_allocated;
}

void* buffer_at(buffer_t* buf, size_t index)
{
	if (index >= buf->size)
		return NULL;
	return (uint8_t*)buf->data + buf->element_size * index;
}

void* buffer_last(buffer_t* buf)
{
	return buffer_at(buf, buf->size - 1);
}

void buffer_splice(buffer_t* buf, size_t start, size_t length)
{
	if (start >= buf->size || length == 0)
		return;
	memmove(
		buffer_at(buf, start),
		buffer_at(buf, start + length),
		(buf->size - (start + length)) * buf->element_size
	);
	buf->size -= length;
}

void buffer_remove(buffer_t* buf, void* element)
{
	for (size_t i = 0; i < buf->size; ++i) {
		// Iterate in reverse
		if (buffer_at(buf, buf->size - i - 1) == element) {
			memmove(
				buffer_at(buf, buf->size - i - 1),
				buffer_at(buf, buf->size - i),
				i * buf->element_size
			);
			buf->size--;
		}
	}
}
