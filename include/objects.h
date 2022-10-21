#pragma once

#include <stdbool.h>

#define IS_ARRAY(x)    (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_ARRAY)
#define IS_FUNCTION(x) (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_FUNCTION)
#define IS_INSTANCE(x) (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_INSTANCE)
#define IS_NATIVE(x)   (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_NATIVE)
#define IS_MODULE(x)   (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_MODULE)
#define IS_RESOURCE(x) (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_RESOURCE)
#define IS_STRING(x)   (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_STRING)
#define IS_TABLE(x)    (IS_OBJECT(x) && AS_OBJECT(x)->type == OBJECT_TABLE)

#define AS_ARRAY(x)    ((array_t*)AS_OBJECT(x))
#define AS_FUNCTION(x) ((function_t*)AS_OBJECT(x))
#define AS_INSTANCE(x) ((instance_t*)AS_OBJECT(x))
#define AS_NATIVE(x)   ((native_t*)AS_OBJECT(x))
#define AS_MODULE(x)   ((module_t*)AS_OBJECT(x))
#define AS_RESOURCE(x) ((resource_t*)AS_OBJECT(x))
#define AS_STRING(x)   ((string_t*)AS_OBJECT(x))
#define AS_TABLE(x)    ((table_t*)AS_OBJECT(x))

typedef enum object_type {
	OBJECT_ARRAY,
	OBJECT_CLASS,
	OBJECT_FUNCTION,
	OBJECT_INSTANCE,
	OBJECT_NATIVE,
	OBJECT_MODULE,
	OBJECT_RESOURCE,
	OBJECT_STRING,
	OBJECT_TABLE,
} object_type_t;

typedef int8_t (*native_fn_t)(vm_t* vm, uint8_t argc);

// -----------------------------------------------------------------------------

typedef struct class class_t;

typedef struct object {
	object_type_t type;
	bool gc_bit;

	class_t* class;

	// Linked list of objects on the heap
	struct object* next;
} object_t;

// -----------------------------------------------------------------------------

typedef struct array {
	object_t header;
	buffer_t values;
} array_t;

array_t* new_array(vm_t* vm);
array_t* new_array_from(vm_t* vm, buffer_t* values);
void free_array(array_t* array);

// -----------------------------------------------------------------------------

typedef struct string {
	object_t header;
	size_t length;
	uint32_t hash;
	char data[];
} string_t;

string_t* new_string(vm_t* vm, const char* str);
string_t* new_string_length(vm_t* vm, const char* str, size_t length);
void free_string(string_t* string);

// -----------------------------------------------------------------------------

typedef enum function_type {
	FUNCTION_COMPILED,
	FUNCTION_NATIVE,
} function_type_t;

typedef struct function {
	object_t header;
	function_type_t type;
	uint8_t arity;
	union {
		struct {
			buffer_t code;
			buffer_t constants;
			// All functions are closures.
			buffer_t captures;
		} compiled;
		native_fn_t native;
	};
} function_t;

function_t* new_function(vm_t* vm, uint8_t arity);
function_t* new_native_function(vm_t* vm, native_fn_t fn, uint8_t arity);
void free_function(function_t* fn);

// -----------------------------------------------------------------------------

typedef struct table_pair {
	value_t key;
	value_t value;
} table_pair_t;

#define TABLE_CAPACITY 16

typedef struct table {
	object_t header;
	buffer_t buckets[TABLE_CAPACITY];
} table_t;

table_t* new_table(vm_t* vm);
void free_table(table_t* table);
value_t table_get(table_t* table, value_t key);
void table_set(table_t* table, value_t key, value_t value);
void table_remove(table_t* table, value_t key);

// -----------------------------------------------------------------------------

typedef struct resource {
	object_t header;
	uint8_t data[];
} resource_t;

// -----------------------------------------------------------------------------

typedef struct module {
	object_t header;
	string_t* name;
} module_t;

// -----------------------------------------------------------------------------

struct class {
	object_t header;
	string_t* name;
	struct class* super;
	buffer_t constants;
	table_t* properties;
};

class_t* new_class(vm_t* vm, class_t* super, string_t* name);
void free_class(class_t* class);

// -----------------------------------------------------------------------------

typedef struct instance {
	object_t header;
	value_t fields[];
} instance_t;
