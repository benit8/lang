#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "config.h"

typedef struct buffer {
	void* data;
	size_t size, element_size, capacity;
} buffer_t;

buffer_t buffer_new(size_t element_size);
void buffer_free(buffer_t* buf);
bool buffer_push(buffer_t* buf, void* element);
void* buffer_at(buffer_t* buf, size_t index);
void* buffer_last(buffer_t* buf);
void buffer_splice(buffer_t* buf, size_t start, size_t length);
void buffer_remove(buffer_t* buf, void* element);
