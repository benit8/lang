#include <assert.h>
#include <stdio.h>
#include "vm.h"

static int8_t print(vm_t* vm, uint8_t argc)
{
	assert(argc >= 1);
	value_t format = vm_pop(vm);
	assert(IS_STRING(format));
	string_t* fmt = (string_t*)AS_OBJECT(format);

	for (size_t i = 0; i < fmt->length; ++i) {
		if (fmt->data[i] == '{' && fmt->data[i + 1] == '}') {
			i++;
			assert(--argc >= 1);
			value_t arg = vm_pop(vm);
			if (IS_NULL(arg)) printf("null");
			else if (IS_BOOL(arg)) printf(arg == VALUE_TRUE ? "true" : "false");
			else if (IS_NUMBER(arg)) printf("%g", AS_NUMBER(arg));
			else if (IS_STRING(arg)) printf("%s", AS_STRING(arg)->data);
			else if (IS_ARRAY(arg)) printf("[(%zu)]", AS_ARRAY(arg)->values.size);
			else printf("[unimplemented printer]");
		} else {
			printf("%c", fmt->data[i]);
		}
	}

	return 0;
}

static int8_t println(vm_t* vm, uint8_t argc)
{
	int8_t res = print(vm, argc);
	printf("\n");
	return res;
}

void vm_std_io(vm_t* vm)
{
	table_set(vm->global, VALUE_OBJECT(new_string(vm, "print")), VALUE_OBJECT(new_native_function(vm, &print, 1)));
	table_set(vm->global, VALUE_OBJECT(new_string(vm, "println")), VALUE_OBJECT(new_native_function(vm, &println, 1)));
}
