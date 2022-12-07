#include "vm.h"

void vm_free(vm_t* vm, object_t* obj)
{
	switch (obj->type) {
		case OBJECT_ARRAY: free_array((array_t*)obj); break;
		case OBJECT_CLASS: free_class((class_t*)obj); break;
		case OBJECT_FUNCTION: free_function((function_t*)obj); break;
		case OBJECT_STRING: {
			string_t* s = (string_t*)obj;
			vm_string_pool_remove(&vm->string_pool, s);
			free_string(s);
		} break;
		case OBJECT_TABLE: free_table((table_t*)obj); break;
		default: break;
	}
}

void vm_gc_keep_alive(vm_t* vm, object_t* obj)
{
	buffer_push(&vm->gc_roots, &obj);
}

void vm_gc_release(vm_t* vm, object_t* obj)
{
	buffer_remove(&vm->gc_roots, obj);
}

static void sweep(object_t* obj);
static inline void sweep_value(value_t value) {
	if (IS_OBJECT(value))
		sweep(AS_OBJECT(value));
}

static void sweep(object_t* obj)
{
	obj->gc_bit = 0;

	switch (obj->type) {
	case OBJECT_ARRAY: {
		array_t* array = (array_t*)obj;
		for (size_t i = 0; i < array->values.size; ++i)
			sweep_value(*(value_t*)buffer_at(&array->values, i));
	} break;
	case OBJECT_CLASS: {
		class_t* class = (class_t*)obj;
		for (size_t i = 0; i < class->constants.size; ++i)
			sweep_value(*(value_t*)buffer_at(&class->constants, i));
		sweep((object_t*)class->properties);
	} break;
	case OBJECT_FUNCTION: {
		function_t* fn = (function_t*)obj;
		for (size_t i = 0; i < fn->compiled.constants.size; ++i)
			sweep_value(*(value_t*)buffer_at(&fn->compiled.constants, i));
	} break;
	case OBJECT_TABLE: {
		table_t* table = (table_t*)obj;
		for (size_t i = 0; i < TABLE_CAPACITY; ++i) {
			for (size_t j = 0; j < table->buckets[i].size; ++j) {
				table_pair_t* p = buffer_at(&table->buckets[i], j);
				sweep_value(p->key);
				sweep_value(p->value);
			}
		}
	} break;
	default:
		break;
	}
}

unsigned vm_gc_collect(vm_t* vm)
{
	// Mark all objects for collection
	for (object_t* cur = vm->heap; cur != NULL; cur = cur->next) {
		cur->gc_bit = 1;
	}

	// Un-mark root objects
	for (size_t i = 0; i < vm->gc_roots.size; ++i) {
		object_t** obj = buffer_at(&vm->gc_roots, i);
		sweep(*obj);
	}

	// Now free the non-root objects
	unsigned collected = 0;
	for (object_t** cur = &vm->heap; *cur != NULL; ) {
		if ((*cur)->gc_bit == 1) {
			object_t* next = (*cur)->next;
			vm_free(vm, *cur);
			*cur = next;
			collected++;
		} else {
			cur = &(*cur)->next;
		}
	}

	return collected;
}
