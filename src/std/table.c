#include <assert.h>
#include "std.h"

static int8_t get(vm_t* vm, uint8_t argc)
{
	assert(argc == 1);
	table_t* this = AS_TABLE(vm_pop(vm));
	value_t value = table_get(this, vm_pop(vm));
	vm_push(vm, value);
	return 1;
}

static int8_t set(vm_t* vm, uint8_t argc)
{
	assert(argc == 2);
	table_t* this = AS_TABLE(vm_pop(vm));
	value_t key = vm_pop(vm);
	value_t value = vm_pop(vm);
	table_set(this, key, value);
	return 0;
}

void vm_std_table(vm_t* vm)
{
	vm->table_class = new_class(vm, NULL, new_string(vm, "Table"));

	DEFINE_METHOD(vm->table_class, "get", get, 1);
	DEFINE_METHOD(vm->table_class, "set", set, 2);
}
