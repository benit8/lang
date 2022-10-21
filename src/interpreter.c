#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "vm.h"

static const char* op_names[] = {
#define __ENUMERATE(op) #op,
	__ENUMERATE_OP_CODES
#undef __ENUMERATE
};

static void runtime_error(vm_t* vm, const char* error, ...)
{
	char buf[128];
	int c = snprintf(buf, sizeof(buf), "runtime error: ");

	va_list ap;
	va_start(ap, error);
	vsnprintf(buf + c, sizeof(buf) - c, error, ap);
	va_end(ap);

	vm->error_handler(buf);
}

static frame_t* push_frame(vm_t* vm, buffer_t* frames, value_t callable, uint8_t argc)
{
	if (!IS_FUNCTION(callable)) {
		runtime_error(vm, "value is not callable");
		value_dump(callable);
		return NULL;
	}

	function_t* fn = (function_t*)AS_OBJECT(callable);
	if (argc < fn->arity) {
		runtime_error(vm, "not enough arguments to run function, got %u instead of %u", argc, fn->arity);
		return NULL;
	}

	frame_t frame;
	frame.stack_start = vm->stack.size - argc;
	if (vm->debug) printf("+++ STACK START IS %zu-%u\n", vm->stack.size, argc);
	frame.callee = fn;

	// When the function is native, point to the instruction that called it, so the argc is available.
	frame.ip = fn->type == FUNCTION_NATIVE ? ((frame_t*)buffer_last(frames))->ip : fn->compiled.code.data;

	buffer_push(frames, &frame);
	return buffer_last(frames);
}

static frame_t* pop_frame(vm_t* vm, buffer_t* frames, int8_t n_returned)
{
	value_t ret = n_returned ? vm_pop(vm) : VALUE_NULL;

	frame_t* f = buffer_last(frames);
	if (f) {
		if (vm->debug) printf("--- STACK_START WAS %zu, RETURNED %d\n", f->stack_start, n_returned);

		// Native functions will consume all arguments on the stackn thus no need to reset its size.
		if (f->callee->type != FUNCTION_NATIVE) {
			vm->stack.size = f->stack_start;
		}
	}
	frames->size--;

	if (n_returned) vm_push(vm, ret);

	return buffer_last(frames);
}

static class_t* get_class(vm_t* vm, value_t value)
{
	if (IS_NULL(value)) return NULL;
	if (IS_BOOL(value)) return vm->bool_class;
	if (IS_NUMBER(value)) return vm->number_class;
	if (IS_ARRAY(value)) return vm->array_class;
	if (IS_FUNCTION(value)) return vm->function_class;
	// if (IS_INSTANCE(value)) return vm->instance_class;
	// if (IS_MODULE(value)) return vm->module_class;
	// if (IS_RESOURCE(value)) return vm->resource_class;
	if (IS_STRING(value)) return vm->string_class;
	if (IS_TABLE(value)) return vm->table_class;
	return NULL;
}

void vm_push(vm_t* vm, value_t value)
{
	if (vm->debug) { printf(">>> [%zu] ", vm->stack.size); value_dump(value); }
	buffer_push(&vm->stack, &value);
}

value_t vm_pop(vm_t* vm)
{
	value_t value = *(value_t*)buffer_last(&vm->stack);
	vm->stack.size--;
	if (vm->debug) printf("<<< [%zu]\n", vm->stack.size);
	return value;
}

static value_t vm_peek(vm_t* vm)
{
	return *(value_t*)buffer_last(&vm->stack);
}

void vm_interpret(vm_t* vm, value_t callable, uint8_t argc)
{
	buffer_t frames = buffer_new(sizeof(frame_t));
	frame_t* f = push_frame(vm, &frames, callable, argc);

#define NEXT() f->ip++; goto start;

start:
	if (f) {
		// Special handling for native functions
		if (f->callee->type == FUNCTION_NATIVE) {
			int8_t res = f->callee->native(vm, f->ip->arg);
			f = pop_frame(vm, &frames, res);
			if (f) f->ip++;
			goto start;
		}

		if (vm->debug) printf("%p%*s %s %d\n", &frames, (int)frames.size * 2, "", op_names[f->ip->op], f->ip->arg);

		switch (f->ip->op) {

		// Do nothing
		case OP_NOP: {
			NEXT();
		}
		// Push a number of null values on the stack
		case OP_PUSH: {
			for (int16_t i = 0; i < f->ip->arg; ++i) {
				vm_push(vm, VALUE_NULL);
			}
			NEXT();
		}
		// Push a 'false' value
		case OP_PUSH_FALSE: {
			vm_push(vm, VALUE_FALSE);
			NEXT();
		}
		// Push a 'true' value
		case OP_PUSH_TRUE: {
			vm_push(vm, VALUE_TRUE);
			NEXT();
		}
		// Push a constant (number, string, instance...) value
		case OP_PUSH_CONST: {
			vm_push(vm, *(value_t*)buffer_at(&f->callee->compiled.constants, f->ip->arg));
			NEXT();
		}
		// Load a value to the stack
		case OP_LOAD: {
			assert(vm->stack.capacity >= f->stack_start + f->ip->arg);
			value_t* vp = &((value_t*)vm->stack.data)[f->stack_start + f->ip->arg];
			vm_push(vm, *vp);
			NEXT();
		}
		// Store a value from the stack
		case OP_STORE: {
			value_t v = vm_peek(vm);
			assert(vm->stack.capacity >= f->stack_start + f->ip->arg);
			value_t* vp = &((value_t*)vm->stack.data)[f->stack_start + f->ip->arg];
			*vp = v;
			NEXT();
		}
		// Load an upvalue to the stack
		case OP_LOAD_UP: {
			vm_push(vm, *(value_t*)buffer_at(&f->callee->compiled.captures, f->ip->arg));
			NEXT();
		}
		// Store an upvalue from the stack
		case OP_STORE_UP: {
			*(value_t*)buffer_at(&f->callee->compiled.captures, f->ip->arg) = vm_pop(vm);
			NEXT();
		}

#define BINARY_OP(name, op) case OP_ ## name: { \
		value_t a = vm_pop(vm); \
		value_t b = vm_pop(vm); \
		if (!IS_NUMBER(a) || !IS_NUMBER(b)) \
			return runtime_error(vm, "operand of " #name " is not a Number"); \
		value_t res = VALUE_NUMBER(AS_NUMBER(a) op AS_NUMBER(b)); \
		vm_push(vm, res); \
		NEXT(); \
	}
		BINARY_OP(ADD, +)
		BINARY_OP(SUB, -)
		BINARY_OP(MUL, *)
		BINARY_OP(DIV, /)
		// BINARY_OP(MOD, %)
		// BINARY_OP(BAND, &)
		// BINARY_OP(BOR, |)
		// BINARY_OP(XOR, ^)
		// BINARY_OP(LSH, <<)
		// BINARY_OP(RSH, >>)
		// BINARY_OP(POW, **)
#undef BINARY_OP

		case OP_INC: {
			value_t a = vm_pop(vm);
			if (!IS_NUMBER(a))
				return runtime_error(vm, "operand of INC is not a Number");
			value_t res = VALUE_NUMBER(AS_NUMBER(a) + 1);
			vm_push(vm, res);
			NEXT();
		}
		case OP_DEC: {
			value_t a = vm_pop(vm);
			if (!IS_NUMBER(a))
				return runtime_error(vm, "operand of DEC is not a Number");
			value_t res = VALUE_NUMBER(AS_NUMBER(a) - 1);
			vm_push(vm, res);
			NEXT();
		}
		case OP_NEG: {
			value_t a = vm_pop(vm);
			if (!IS_NUMBER(a))
				return runtime_error(vm, "operand of NEG is not a Number");
			value_t res = VALUE_NUMBER(-AS_NUMBER(a));
			vm_push(vm, res);
			NEXT();
		}
		case OP_EQ: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			// FIXME: compare types first?
			value_t res = VALUE_BOOL(value_equals(a, b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_NEQ: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			// FIXME: compare types first?
			value_t res = VALUE_BOOL(!value_equals(a, b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_GT: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_NUMBER(a) || !IS_NUMBER(b))
				return runtime_error(vm, "operand of GT is not a Number");
			value_t res = VALUE_BOOL(AS_NUMBER(a) > AS_NUMBER(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_GTE: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_NUMBER(a) || !IS_NUMBER(b))
				return runtime_error(vm, "operand of GTE is not a Number");
			value_t res = VALUE_BOOL(AS_NUMBER(a) >= AS_NUMBER(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_LT: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_NUMBER(a) || !IS_NUMBER(b))
				return runtime_error(vm, "operand of LT is not a Number");
			value_t res = VALUE_BOOL(AS_NUMBER(a) < AS_NUMBER(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_LTE: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_NUMBER(a) || !IS_NUMBER(b))
				return runtime_error(vm, "operand of LTE is not a Number");
			value_t res = VALUE_BOOL(AS_NUMBER(a) <= AS_NUMBER(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_CMP: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_NUMBER(a) || !IS_NUMBER(b))
				return runtime_error(vm, "operand of CMP is not a Number");
			value_t res = VALUE_NUMBER(AS_NUMBER(a) - AS_NUMBER(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_AND: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_BOOL(a) || !IS_BOOL(b))
				return runtime_error(vm, "operand of AND is not a Bool");
			value_t res = VALUE_BOOL(AS_BOOL(a) && AS_BOOL(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_OR: {
			value_t a = vm_pop(vm);
			value_t b = vm_pop(vm);
			if (!IS_BOOL(a) || !IS_BOOL(b))
				return runtime_error(vm, "operand of OR is not a Bool");
			value_t res = VALUE_BOOL(AS_BOOL(a) || AS_BOOL(b));
			vm_push(vm, res);
			NEXT();
		}
		case OP_NOT: {
			value_t a = vm_pop(vm);
			if (!IS_BOOL(a))
				return runtime_error(vm, "operand of NOT is not a Bool");
			value_t res = VALUE_BOOL(!AS_BOOL(a));
			vm_push(vm, res);
			NEXT();
		}
		// case OP_BNOT: {
		// 	value_t a = vm_pop(vm);
		// 	if (!IS_NUMBER(a))
		// 		return runtime_error(vm, "operand of BNOT is not a Number");
		// 	value_t res = VALUE_NUMBER(~((uint64_t)AS_NUMBER(a)));
		// 	vm_push(vm, res);
		// 	NEXT();
		// }

		// Get a value from the global object
		case OP_GETG: {
			value_t key = vm_pop(vm);
			value_t value = table_get(vm->global, key);
			if (value == VALUE_NULL) return runtime_error(vm, "undefined variable '%s'", ((string_t*)AS_OBJECT(key))->data);
			vm_push(vm, value);
			NEXT();
		}
		// Get a property from a value
		case OP_GETP: {
			value_t this = vm_pop(vm);
			value_t prop_name = vm_pop(vm);
			assert(IS_STRING(prop_name));
			class_t* class = get_class(vm, this);
			assert(class);
			value_t prop_value = table_get(class->properties, prop_name);
			if (prop_value == VALUE_NULL) return runtime_error(vm, "undefined property '%s' on value of type '%s'", (string_t*)AS_OBJECT(prop_name), class->name);
			// Insert `this` value into stack for methods calls
			if (IS_FUNCTION(prop_value) && f->ip[1].op == OP_CALL)
				vm_push(vm, this);
			vm_push(vm, prop_value);
			NEXT();
		}
		// Register upvalues into a function's captures
		case OP_CLOSE: {
			value_t fn_v = vm_pop(vm);
			function_t* fn = (function_t*)AS_OBJECT(fn_v);
			for (int i = 0; i < f->ip->arg; ++i) {
				value_t upv = vm_pop(vm);
				buffer_push(&fn->compiled.captures, &upv);
			}
			vm_push(vm, fn_v);
			NEXT();
		}
		// Call a function
		case OP_CALL: {
			value_t callee = vm_pop(vm);
			f = push_frame(vm, &frames, callee, f->ip->arg);
			goto start;
		}
		// Return from a function
		case OP_RETURN: {
			uint8_t n_return = f->ip->arg;
			f = pop_frame(vm, &frames, n_return);
			if (f) f->ip++;
			goto start;
		}
		// Jump
		case OP_JUMP: {
			f->ip += f->ip->arg;
			goto start;
		}
		// Jump if value is false
		case OP_JUMP_IF: {
			value_t truth = vm_pop(vm);
			if (!IS_BOOL(truth)) return runtime_error(vm, "condition did not result in a boolean");
			f->ip += AS_BOOL(truth) ? 1 : f->ip->arg;
			goto start;
		}

		default: {
			printf("Unimplemented OP code (%d)\n", f->ip->op);
			break;
		}

		}
	}

#undef NEXT

	assert(f == NULL);
	buffer_free(&frames);
}
