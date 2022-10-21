#include <stdio.h>
#include <string.h>
#include "vm.h"

static uint64_t hash64(uint64_t n)
{
	n = (~n) + (n << 18); // n = (n << 18) - n - 1;
	n = n ^ (n >> 31);
	n = n * 21; // n = (n + (n << 2)) + (n << 4);
	n = n ^ (n >> 11);
	n = n + (n << 6);
	n = n ^ (n >> 22);
	return n;
}

uint64_t value_hash(value_t value)
{
	if (IS_STRING(value))
		return ((string_t*)AS_OBJECT(value))->hash;
	return hash64(value);
}

bool value_equals(value_t a, value_t b)
{
	if (IS_NUMBER(a) != IS_NUMBER(b) || (a & TYPE_MASK) != (b & TYPE_MASK))
		return false;

	switch (a & TYPE_MASK) {
	case TYPE_NULL:
	case TYPE_BOOL:
		return a == b;
	case TYPE_OBJECT: {
		object_t* obja = AS_OBJECT(a);
		object_t* objb = AS_OBJECT(b);
		if (obja->type != objb->type)
			return false;
		switch (obja->type) {
		case OBJECT_STRING:
			return strcmp(((string_t*)obja)->data, ((string_t*)objb)->data) == 0;
		default:
			return obja == objb;
		}
	} break;
	}

	return false;
}
