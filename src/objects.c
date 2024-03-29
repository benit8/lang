#include <assert.h>
#include <string.h>
#include "vm.h"

// Objects ---------------------------------------------------------------------

static void init_header(vm_t* vm, object_t* obj, object_type_t type, class_t* class)
{
	obj->type = type;
	obj->gc_bit = false;
	obj->class = class;
	obj->next = vm->heap;
	vm->heap = obj;
}

// Array -----------------------------------------------------------------------

array_t* new_array(vm_t* vm)
{
	array_t* array = ALLOC(sizeof(array_t));
	init_header(vm, &array->header, OBJECT_ARRAY, vm->array_class);
	array->values = buffer_new(sizeof(value_t));
	return array;
}

array_t* new_array_from(vm_t* vm, buffer_t* values)
{
	array_t* array = new_array(vm);
	for (size_t i = 0; i < values->size; ++i)
		buffer_push(&array->values, buffer_at(values, i));
	return array;
}

void free_array(array_t* array)
{
	buffer_free(&array->values);
	FREE(array);
}

// String ---------------------------------------------------------------------

string_t* new_string(vm_t* vm, const char* str)
{
	return new_string_length(vm, str, strlen(str));
}

string_t* new_string_length(vm_t* vm, const char* str, size_t length)
{
	string_t** bucket = vm_lookup_string_pool(&vm->string_pool, str, length);
	assert(bucket != NULL);

	string_t* string = *bucket;

	if (string->length == 0) {
		// String was just allocated by the pool
		init_header(vm, &string->header, OBJECT_STRING, vm->string_class);
		memcpy(string->data, str, length);
		string->data[length] = 0;
		string->length = length;
	}

	return string;
}

void free_string(string_t* string)
{
	FREE(string);
}

bool string_compare(string_t* a, string_t* b)
{
	return strncmp(a->data, b->data, a->length < b->length ? a->length : b->length) == 0;
}

// Function --------------------------------------------------------------------

function_t* new_function(vm_t* vm, uint8_t arity)
{
	function_t* fn = ALLOC(sizeof(function_t));
	init_header(vm, &fn->header, OBJECT_FUNCTION, vm->function_class);
	fn->type = FUNCTION_COMPILED;
	fn->arity = arity;
	fn->compiled.code = buffer_new(sizeof(op_t));
	fn->compiled.constants = buffer_new(sizeof(value_t));
	fn->compiled.captures = buffer_new(sizeof(value_t));
	return fn;
}

function_t* new_native_function(vm_t* vm, native_fn_t native, uint8_t arity)
{
	function_t* fn = ALLOC(sizeof(function_t));
	init_header(vm, &fn->header, OBJECT_FUNCTION, vm->function_class);
	fn->type = FUNCTION_NATIVE;
	fn->arity = arity;
	fn->native = native;
	return fn;
}

void free_function(function_t* fn)
{
	if (fn->type == FUNCTION_COMPILED) {
		buffer_free(&fn->compiled.code);
		buffer_free(&fn->compiled.constants);
		buffer_free(&fn->compiled.captures);
	}
	FREE(fn);
}

// Table -----------------------------------------------------------------------

static table_pair_t* get_pair(table_t* table, value_t key, bool should_allocate)
{
	uint64_t index = value_hash(key) % TABLE_CAPACITY;
	buffer_t* bucket = &table->buckets[index];

	if (should_allocate && bucket->element_size == 0) {
		*bucket = buffer_new(sizeof(table_pair_t));
	}

	buffer_foreach(*bucket, table_pair_t, p) {
		if (value_equals(p->key, key))
			return p;
	}
	return NULL;
}

table_t* new_table(vm_t* vm)
{
	table_t* table = ALLOC(sizeof(table_t));
	init_header(vm, &table->header, OBJECT_TABLE, vm->table_class);
	return table;
}

void free_table(table_t* table)
{
	for (size_t i = 0; i < TABLE_CAPACITY; ++i)
		buffer_free(&table->buckets[i]);
	FREE(table);
}

value_t table_get(table_t* table, value_t key)
{
	table_pair_t* p = get_pair(table, key, false);
	return p ? p->value : VALUE_NULL;
}

void table_set(table_t* table, value_t key, value_t value)
{
	table_pair_t* p = get_pair(table, key, true);
	if (p != NULL) {
		p->value = value;
		return;
	}

	uint64_t index = value_hash(key) % TABLE_CAPACITY;
	table_pair_t pair = { key, value };
	buffer_push(&table->buckets[index], &pair);
}

void table_remove(table_t* table, value_t key)
{
	table_set(table, key, VALUE_NULL);
}

// Resource --------------------------------------------------------------------

// Module --------------------------------------------------------------------

// Class -----------------------------------------------------------------------

class_t* new_class(vm_t* vm, class_t* super, string_t* name)
{
	class_t* class = ALLOC(sizeof(class_t));
	init_header(vm, &class->header, OBJECT_CLASS, NULL);
	class->name = name;
	class->super = super;
	class->constants = buffer_new(sizeof(value_t));
	class->properties = new_table(vm);
	return class;
}

void free_class(class_t* class)
{
	buffer_free(&class->constants);
	FREE(class);
}

// Instance --------------------------------------------------------------------
