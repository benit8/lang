#include <assert.h>
#include <stdio.h>
#include "vm.h"
#include "std.h"

void vm_std_all(vm_t* vm)
{
	vm_std_array(vm);
	// vm_std_bool(vm);
	vm_std_io(vm);
	// vm_std_net(vm);
	// vm_std_string(vm);
	// vm_std_sys(vm);
	vm_std_table(vm);

	vm->bool_class = new_class(vm, NULL, new_string(vm, "Bool"));
	vm->function_class = new_class(vm, NULL, new_string(vm, "Function"));
	vm->number_class = new_class(vm, NULL, new_string(vm, "Number"));
	vm->string_class = new_class(vm, NULL, new_string(vm, "String"));
}

vm_t* vm_open(char** environment, error_handler_t error)
{
	vm_t* vm = ALLOC(sizeof(vm_t));
	if (!vm) {
		return NULL;
	}

	vm->environment = environment;
	vm->error_handler = error;

	vm->heap = NULL;
	vm->gc_roots = buffer_new(sizeof(object_t*));
	vm->global = new_table(vm);
	vm_gc_keep_alive(vm, (object_t*)vm->global);

	vm->stack = buffer_new(sizeof(value_t));

	return vm;
}

void vm_destroy(vm_t* vm)
{
	// Free the root object list so we collect ALL objects
	buffer_free(&vm->gc_roots);
	// Collect before freeing the heap
	vm_gc_collect(vm);
	vm->heap = NULL;

	buffer_free(&vm->stack);

	FREE(vm);
}
