#pragma once

#include "vm.h"

#define DEFINE_METHOD(class, name, fn, arity) table_set(class->properties, VALUE_OBJECT(new_string(vm, name)), VALUE_OBJECT(new_native_function(vm, fn, arity)));

void vm_std_all(vm_t* vm);

void vm_std_array(vm_t* vm);
void vm_std_bool(vm_t* vm);
void vm_std_io(vm_t* vm);
void vm_std_number(vm_t* vm);
void vm_std_string(vm_t* vm);
void vm_std_table(vm_t* vm);
