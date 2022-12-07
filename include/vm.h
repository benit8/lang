#pragma once

typedef struct vm vm_t;

#include <stddef.h>
#include "buffer.h"
#include "value.h"
#include "objects.h"
#include "vm/op_codes.h"

typedef void (*error_handler_t)(const char* message);

typedef struct {
	string_t** buckets;
	size_t capacity, count;
} string_pool_t;

struct vm {
	char** arguments;
	char** environment;
	bool debug;
	error_handler_t error_handler;

	object_t* heap;
	buffer_t gc_roots;
	table_t* global;
	string_pool_t string_pool;

	buffer_t stack;

	// FIXME: make a class registrar
	class_t* array_class;
	class_t* bool_class;
	class_t* function_class;
	class_t* number_class;
	class_t* string_class;
	class_t* table_class;
};

typedef struct frame {
	function_t* callee;
	size_t stack_start;
	op_t* ip;
} frame_t;

vm_t* vm_open(char** environment, error_handler_t error);
void vm_destroy(vm_t* vm);

value_t vm_compile(vm_t* vm, const char* source, const char* module);
void vm_interpret(vm_t* vm, value_t callable, uint8_t argc);

void vm_push(vm_t* vm, value_t value);
value_t vm_pop(vm_t* vm);

void vm_free(vm_t* vm, object_t* obj);
void vm_gc_keep_alive(vm_t* vm, object_t* obj);
void vm_gc_release(vm_t* vm, object_t* obj);
unsigned vm_gc_collect(vm_t* vm);

void vm_init_string_pool(string_pool_t* sp, size_t capacity);
void vm_free_string_pool(string_pool_t* sp);
string_t** vm_lookup_string_pool(string_pool_t* sp, const char* str, size_t length);
void vm_string_pool_remove(string_pool_t* sp, string_t* string);
