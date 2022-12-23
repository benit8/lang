#include <assert.h>
#include <stdio.h>
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "vm.h"

static op_code_t binary_op(token_type_t token)
{
	if (token == TOKEN_AMPERSAND) return OP_BAND;
	if (token == TOKEN_AMPERSAND_AMPERSAND) return OP_AND;
	if (token == TOKEN_ASTERISK) return OP_MUL;
	if (token == TOKEN_ASTERISK_ASTERISK) return OP_POW;
	if (token == TOKEN_CARET) return OP_XOR;
	if (token == TOKEN_EQUALS_EQUALS) return OP_EQ;
	if (token == TOKEN_EXCLAMATION_EQUALS) return OP_NEQ;
	if (token == TOKEN_GREATER) return OP_GT;
	if (token == TOKEN_GREATER_EQUALS) return OP_GTE;
	if (token == TOKEN_GREATER_GREATER) return OP_RSH;
	if (token == TOKEN_LESS) return OP_LT;
	if (token == TOKEN_LESS_EQUALS) return OP_LTE;
	if (token == TOKEN_LESS_EQUALS_GREATER) return OP_CMP;
	if (token == TOKEN_LESS_LESS) return OP_LSH;
	if (token == TOKEN_MINUS) return OP_SUB;
	if (token == TOKEN_PERCENT) return OP_MOD;
	if (token == TOKEN_PIPE) return OP_BOR;
	if (token == TOKEN_PIPE_PIPE) return OP_OR;
	if (token == TOKEN_PLUS) return OP_ADD;
	if (token == TOKEN_SLASH) return OP_DIV;
	assert(false);
	return OP_NOP;
}

static inline op_t* emit_arg(function_t* fn, op_code_t op, uint8_t arg) {
	op_t o = { op, arg };
	buffer_push(&fn->compiled.code, &o);
	return buffer_last(&fn->compiled.code);
}

static inline op_t* emit(function_t* fn, op_code_t op) {
	return emit_arg(fn, op, 0);
}

static inline size_t emit_jump(function_t* fn, op_code_t op) {
	size_t start = fn->compiled.code.size;
	emit_arg(fn, op, 0);
	return start;
}

static inline void patch_jump(function_t* fn, size_t jump_start) {
	op_t* op = buffer_at(&fn->compiled.code, jump_start);
	op->arg = fn->compiled.code.size - jump_start;
}

static size_t add_constant(function_t* fn, value_t constant)
{
	for (size_t i = 0; i < fn->compiled.constants.size; ++i)
		if (*(value_t*)buffer_at(&fn->compiled.constants, i) == constant)
			return i;
	buffer_push(&fn->compiled.constants, &constant);
	return fn->compiled.constants.size - 1;
}

static inline bool token_equals(token_t* a, token_t* b) {
	return a->type == b->type && a->type == TOKEN_IDENTIFIER && a->index == b->index;
}

static size_t scope_find_local(scope_t* scope, token_t* t)
{
	for (size_t i = 0; i < scope->locals.size; ++i) {
		if (token_equals(t, buffer_at(&scope->locals, i)))
			return i;
	}

	for (size_t i = 0; i < scope->upvalues.size; ++i) {
		if (token_equals(t, buffer_at(&scope->upvalues, i)))
			return i | UPVALUE_MASK;
	}

	return NOT_FOUND;
}

static void compile(vm_t* vm, function_t* fn, ast_node_t* node, scope_t* scope)
{
	switch (node->type) {
	case AST_BINARY:
		compile(vm, fn, node->binary.rhs, scope);
		compile(vm, fn, node->binary.lhs, scope);
		emit(fn, binary_op(node->binary.operator));
		break;
	case AST_BLOCK:
		buffer_foreach(node->block.body, ast_node_t*, child) {
			compile(vm, fn, *child, node->block.scope);
		}
		if (fn->compiled.code.size == 0 || ((op_t*)buffer_at(&fn->compiled.code, fn->compiled.code.size - 1))->op != OP_RETURN)
			emit(fn, OP_RETURN);
		break;
	case AST_BRANCH: {
		compile(vm, fn, node->branch.condition, scope);
		size_t if_jump = emit_jump(fn, OP_JUMP_IF);
		compile(vm, fn, node->branch.consequent, scope);
		size_t else_jump = emit_jump(fn, OP_JUMP);
		patch_jump(fn, if_jump);
		compile(vm, fn, node->branch.alternate, scope);
		patch_jump(fn, else_jump);
	}	break;
	case AST_CALL:
		for (size_t i = 0; i < node->call.arguments.size; ++i)
			compile(vm, fn, *(ast_node_t**)buffer_at(&node->call.arguments, node->call.arguments.size - i - 1), scope);
		compile(vm, fn, node->call.callee, scope);
		emit_arg(fn, OP_CALL, node->call.arguments.size);
		break;
	case AST_FUNCTION: {
		function_t* inner_fn = new_function(vm, node->function.parameters.size);
		size_t index = add_constant(fn, VALUE_OBJECT(inner_fn));
		compile(vm, inner_fn, node->function.body, scope);
		emit_arg(fn, OP_PUSH_CONST, index);
		scope_t* fn_scope = node->function.body->block.scope;
		if (fn_scope->upvalues.size > 0) {
			for (size_t i = 0; i < fn_scope->upvalues.size; ++i) {
				size_t index = scope_find_local(scope, buffer_at(&fn_scope->upvalues, fn_scope->upvalues.size - i - 1));
				emit_arg(fn,
					(index & UPVALUE_MASK) == UPVALUE_MASK ? OP_LOAD_UP : OP_LOAD,
					(index & UPVALUE_MASK) == UPVALUE_MASK ? index & ~UPVALUE_MASK : index
				);
			}
			emit_arg(fn, OP_PUSH_CONST, index);
			emit_arg(fn, OP_CLOSE, fn_scope->upvalues.size);
		}
	}	break;
	case AST_IDENTIFIER: {
		size_t index = scope_find_local(scope, &node->identifier.token);
		if (index == NOT_FOUND) {
			emit_arg(fn, OP_PUSH_CONST, add_constant(fn, VALUE_OBJECT(new_string(vm, node->identifier.id->name))));
			emit(fn, OP_GETG);
		} else if ((index & UPVALUE_MASK) == UPVALUE_MASK) {
			emit_arg(fn, OP_LOAD_UP, index & ~UPVALUE_MASK);
		} else {
			emit_arg(fn, OP_LOAD, index);
		}
	}	break;
	case AST_LITERAL: {
		switch (node->literal.type) {
		case TOKEN_NULL: emit_arg(fn, OP_PUSH, 1); break;
		case TOKEN_FALSE: emit(fn, OP_PUSH_FALSE); break;
		case TOKEN_TRUE: emit(fn, OP_PUSH_TRUE); break;
		case TOKEN_NUMBER:
			emit_arg(fn, OP_PUSH_CONST, add_constant(fn, VALUE_NUMBER(node->literal.lit->number)));
			break;
		case TOKEN_STRING:
			emit_arg(fn, OP_PUSH_CONST, add_constant(fn, VALUE_OBJECT(new_string_length(vm, node->literal.lit->string.start, node->literal.lit->string.length))));
			break;
		default: break;
		}
	}	break;
	case AST_PROPERTY:
		// TODO: implement ?.
		emit_arg(fn, OP_PUSH_CONST, add_constant(fn, VALUE_OBJECT(new_string(vm, node->property.name->name))));
		compile(vm, fn, node->property.lhs, scope);
		emit(fn, OP_GETP);
		break;
	case AST_RETURN:
		if (node->ret.expression)
			compile(vm, fn, node->ret.expression, scope);
		emit_arg(fn, OP_RETURN, node->ret.expression ? 1 : 0);
		break;
	case AST_UNARY: printf("AST_UNARY\n");
		break;
	case AST_VAR_DECL: {
		size_t index = scope_find_local(scope, &node->var.identifier);
		assert(index != UPVALUE_MASK);
		compile(vm, fn, node->var.initializer, scope);
		emit_arg(fn, OP_STORE, index);
	}	break;
	}
}

value_t vm_compile(vm_t* vm, const char* source, const char* module)
{
	parser_t parser = parse(vm, source, module);
	if (!parser.root) {
		return VALUE_NULL;
	}

	if (vm->debug) parser_dump_node(&parser, parser.root, 0);

	function_t* fn = new_function(vm, 0);
	vm_gc_keep_alive(vm, (object_t*) fn);

	compile(vm, fn, parser.root, parser.scope);

	parser_free(&parser);
	return VALUE_OBJECT(fn);
}
