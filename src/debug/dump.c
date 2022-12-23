#include <stdarg.h>
#include <stdio.h>
#include "vm.h"

static const char* op_names[] = {
#define __ENUMERATE(op) #op,
	__ENUMERATE_OP_CODES
#undef __ENUMERATE
};

static inline bool op_has_arg(op_code_t op) {
	switch (op) {
		case OP_PUSH:
		case OP_PUSH_CONST:
		case OP_LOAD:
		case OP_STORE:
		case OP_LOAD_UP:
		case OP_STORE_UP:
		case OP_CLOSE:
		case OP_CALL:
		case OP_RETURN:
		case OP_JUMP:
		case OP_JUMP_IF:
		case OP_MAKE_ARRAY:
		case OP_MAKE_TABLE:
			return true;
		default: break;
	}
	return false;
}

#define iprintf(indent, fmt, ...) printf("%*s" fmt, indent * 2, "", ##__VA_ARGS__)

static void dump(value_t value, int indent);

static void dump_object(object_t* obj, int indent)
{
	switch (obj->type) {
	case OBJECT_ARRAY: {
		array_t* array = (array_t*) obj;
		iprintf(indent, "Array (%zu) {\n", array->values.size);
		buffer_foreach(array->values, value_t, it)
			dump(*it, indent + 1);
		iprintf(indent, "}\n");
	} break;
	case OBJECT_CLASS: {} break;
	case OBJECT_FUNCTION: {
		function_t* function = (function_t*) obj;
		iprintf(indent, "Function (%u) ", function->arity);
		if (function->type == FUNCTION_NATIVE)
			printf("Native %p\n", function->native);
		else {
			printf("{\n");
			for (size_t i = 0; i < function->compiled.constants.size; ++i) {
				iprintf(indent + 1, "+ %zu ", i);
				dump(*(value_t*)buffer_at(&function->compiled.constants, i), indent + 1);
			}
			for (size_t i = 0; i < function->compiled.code.size; ++i) {
				op_t* op = buffer_at(&function->compiled.code, i);
				iprintf(indent + 1, "> %04zu %-12s", i, op_names[op->op]);
				if (op_has_arg(op->op))
					printf("%d", op->arg);
				printf("\n");
			}
			iprintf(indent, "}\n");
		}
	} break;
	case OBJECT_INSTANCE: {} break;
	case OBJECT_MODULE: {} break;
	case OBJECT_NATIVE:
		iprintf(indent, "Native %p\n", obj);
		break;
	case OBJECT_RESOURCE:
		iprintf(indent, "Resource %p\n", obj);
		break;
	case OBJECT_STRING: {
		string_t* string = (string_t*) obj;
		iprintf(indent, "String (%zu) \"%s\"\n", string->length, string->data);
	} break;
	case OBJECT_TABLE: {
		table_t* table = (table_t*) obj;
		iprintf(indent, "Table {\n");
		for (size_t i = 0; i < TABLE_CAPACITY; ++i) {
			buffer_foreach(table->buckets[i], table_pair_t, p) {
				dump(p->key, indent + 1);
				iprintf(indent + 2, "=> ");
				dump(p->value, 0);
			}
		}
		iprintf(indent, "}\n");
	} break;
	}
}

static void dump(value_t value, int indent)
{
	if (IS_NULL(value))
		iprintf(indent, "null\n");
	else if (IS_BOOL(value))
		iprintf(indent, "%s\n", value == VALUE_TRUE ? "true" : "false");
	else if (IS_NUMBER(value))
		iprintf(indent, "Number %g\n", AS_NUMBER(value));
	else if (IS_OBJECT(value))
		dump_object(AS_OBJECT(value), indent);
	else
		iprintf(indent, "(Unknown value)\n");
}

void value_dump(value_t value)
{
	dump(value, 0);
}
