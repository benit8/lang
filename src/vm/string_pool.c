#include <assert.h>
#include <string.h>
#include "vm.h"

static uint32_t fnv1_hash_data(const void *data, size_t size)
{
	uint32_t hash = 2166136261ul;
	const uint8_t *bytes = (uint8_t *)data;

	while (size-- != 0) {
		hash ^= *bytes;
		hash = hash * 0x01000193;
		bytes++;
	}

	return hash;
}

static inline bool should_rehash(string_pool_t* sp) {
	return (sp->count + 1) * 100 >= sp->capacity * HASH_LOAD_FACTOR;
}

static uint32_t double_hash(uint32_t h)
{
	if (h == 0xBA5EDB01)
		return 0u;
	if (h == 0u)
		h = 0xBA5EDB01;

	h ^= h << 13;
	h ^= h >> 17;
	h ^= h << 5;
	return h;
}

static void rehash(string_pool_t* sp, size_t new_capacity)
{
	if (new_capacity < sp->capacity)
		return;

	string_t** new_buckets = ALLOC(new_capacity * sizeof(string_t*));

	for (size_t i = 0; i < sp->capacity; ++i) {
		string_t** b = sp->buckets + i;
		if (*b != NULL) {
			uint32_t h = (*b)->hash;
			for (;;) {
				uint32_t index = h % new_capacity;
				string_t** new_bucket = new_buckets + index;
				if (*new_bucket == NULL) {
					*new_bucket = *b;
					break;
				}
				h = double_hash(h);
			}
		}
	}

	FREE(sp->buckets);
	sp->buckets = new_buckets;
	sp->capacity = new_capacity;
}

void vm_init_string_pool(string_pool_t* sp, size_t capacity)
{
	sp->capacity = capacity;
	sp->count = 0;
	sp->buckets = ALLOC(capacity * sizeof(string_t*));
}

void vm_free_string_pool(string_pool_t* sp)
{
	sp->capacity = 0;
	sp->count = 0;
	FREE(sp->buckets);
}

string_t** vm_lookup_string_pool(string_pool_t* sp, const char* str, size_t length)
{
	uint32_t str_hash = fnv1_hash_data(str, length);

	if (should_rehash(sp)) {
		rehash(sp, sp->capacity * 2);
	}

	uint32_t h = str_hash;
	for (;;) {
		uint32_t index = h % sp->capacity;
		string_t** bucket = sp->buckets + index;

		// Bucket is free
		if (*bucket == NULL) {
			*bucket = ALLOC(sizeof(string_t) + length + 1);
			(*bucket)->hash = str_hash;
			sp->count++;
			return bucket;
		}
		// Bucket is allocated (= in use)
		if ((*bucket)->hash == str_hash && strncmp((*bucket)->data, str, length) == 0) {
			return bucket;
		}

		uint32_t g = double_hash(h);
		assert(g != h);
		h = g;
	}

	return NULL;
}

void vm_string_pool_remove(string_pool_t* sp, string_t* string)
{
	uint32_t hash = string->hash;
	for (;;) {
		uint32_t index = hash % sp->capacity;
		string_t** bucket = sp->buckets + index;

		// `string_t` pointers should be unique, thus we can strictly compare them
		if (*bucket == string) {
			*bucket = NULL;
			sp->count--;
			// Deallocation occurs in the GC
			return;
		}

		hash = double_hash(hash);
	}
}
