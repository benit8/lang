#include <assert.h>
#include "std.h"

static int8_t array_at(vm_t* vm, uint8_t argc)
{
	assert(argc == 1);
	array_t* this = AS_ARRAY(vm_pop(vm));
	value_t index = vm_pop(vm);
	value_t* value = buffer_at(&this->values, AS_NUMBER(index));
	assert(value);
	vm_push(vm, value ? *value : VALUE_NULL);
	return 1;
}

static int8_t array_each(vm_t* vm, uint8_t argc)
{
	assert(argc == 1);
	array_t* this = AS_ARRAY(vm_pop(vm));
	value_t callback = vm_pop(vm);
	assert(IS_FUNCTION(callback));

	for (size_t i = 0; i < this->values.size; ++i) {
		vm_push(vm, *(value_t*)buffer_at(&this->values, i));
		vm_interpret(vm, callback, 1);
	}
	return 0;
}

static int8_t range(vm_t* vm, uint8_t argc)
{
	assert(argc == 2 || argc == 3);
	value_t min = vm_pop(vm);
	value_t max = vm_pop(vm);
	assert(IS_NUMBER(min) && IS_NUMBER(max));
	value_t step = VALUE_NUMBER(1);
	if (argc == 3) {
		step = vm_pop(vm);
		assert(IS_NUMBER(step));
	}

	array_t* a = new_array(vm);
	for (double i = AS_NUMBER(min); i < AS_NUMBER(max); i += AS_NUMBER(step)) {
		value_t it = VALUE_NUMBER(i);
		buffer_push(&a->values, &it);
	}

	vm_push(vm, VALUE_OBJECT(a));
	return 1;
}

void vm_std_array(vm_t* vm)
{
	vm->array_class = new_class(vm, NULL, new_string(vm, "Array"));

	DEFINE_METHOD(vm->array_class, "at", array_at, 1);
	DEFINE_METHOD(vm->array_class, "each", array_each, 1);

	table_set(vm->global, VALUE_OBJECT(new_string(vm, "range")), VALUE_OBJECT(new_native_function(vm, &range, 2)));
}
