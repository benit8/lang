#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "compiler/parser.h"
#include "vm.h"

#define EXPECT(TYPE, WHAT, ...) if (!consumes(vm, p, TOKEN_##TYPE)) { parse_error(vm, p, "expected " WHAT, ##__VA_ARGS__); return NULL; }

static inline void parse_error(vm_t* vm, parser_t* p, const char* error, ...)
{
	char buf[128];
	int c = snprintf(buf, sizeof(buf), "parse error in %s at %u,%u: ", p->lexer.module, p->current.line, p->current.column);

	va_list ap;
	va_start(ap, error);
	vsnprintf(buf + c, sizeof(buf) - c, error, ap);
	va_end(ap);

	vm->error_handler(buf);
}

static void free_node(ast_node_t* node)
{
	switch (node->type) {
	case AST_BINARY:
		free_node(node->binary.lhs);
		free_node(node->binary.rhs);
		break;
	case AST_BLOCK:
		buffer_free(&node->block.scope->upvalues);
		buffer_free(&node->block.scope->locals);
		FREE(node->block.scope);
		for (size_t i = 0; i < node->block.body.size; ++i)
			free_node(*((ast_node_t**)buffer_at(&node->block.body, i)));
		buffer_free(&node->block.body);
		break;
	case AST_BRANCH:
		free_node(node->branch.condition);
		free_node(node->branch.consequent);
		free_node(node->branch.alternate);
		break;
	case AST_CALL:
		free_node(node->call.callee);
		for (size_t i = 0; i < node->call.arguments.size; ++i)
			free_node(*((ast_node_t**)buffer_at(&node->call.arguments, i)));
		buffer_free(&node->call.arguments);
		break;
	case AST_FUNCTION:
		buffer_free(&node->function.parameters);
		free_node(node->function.body);
		break;
	case AST_PROPERTY:
		free_node(node->property.lhs);
		break;
	case AST_RETURN:
		free_node(node->ret.expression);
		break;
	case AST_UNARY:
		free_node(node->unary.lhs);
		break;
	case AST_VAR_DECL:
		free_node(node->var.initializer);
		break;
	default:
		break;
	}

	FREE(node);
}

static inline bool token_equals(token_t* a, token_t* b) {
	return a->type == b->type && a->type == TOKEN_IDENTIFIER && a->index == b->index;
}

static size_t scope_add_local(scope_t* scope, token_t* t)
{
	for (size_t i = 0; i < scope->locals.size; ++i) {
		if (token_equals(buffer_at(&scope->locals, i), t))
			return NOT_FOUND;
	}
	buffer_push(&scope->locals, t);
	return scope->locals.size - 1;
}

static size_t scope_find_local_or_upvalue(scope_t* scope, token_t* t)
{
	for (size_t i = 0; i < scope->locals.size; ++i) {
		token_t* test = buffer_at(&scope->locals, i);
		if (token_equals(test, t))
			return i;
	}

	for (size_t i = 0; i < scope->upvalues.size; ++i) {
		token_t* test = buffer_at(&scope->upvalues, i);
		if (token_equals(test, t))
			return i | UPVALUE_MASK;
	}

	if (scope->parent) {
		size_t index = scope_find_local_or_upvalue(scope->parent, t);
		if (index != NOT_FOUND) {
			buffer_push(&scope->upvalues, t);
			return (scope->upvalues.size - 1) | UPVALUE_MASK;
		}
	}

	// FIXME: Lookup global object?
	// value_t sym = new_string_length(vm, );
	// value_t val = table_get(vm->global, sym);
	// vm_free(sym); // Must free but the heap linked list is kinda shit
	// if (val != VALUE_NULL) {
	// 	buffer_push(&scope->upvalues, t);
	// 	return (scope->upvalues.size - 1) | UPVALUE_MASK;
	// }

	return NOT_FOUND;
}

#include "parser/ast.c"

static inline token_type_t peek(parser_t* p) {
	return p->current.type;
}

static inline token_t consume(vm_t* vm, parser_t* p) {
	p->previous = p->current;
	p->current = lexer_next(vm, &p->lexer);
	// lexer_dump_token(&p->lexer, &p->previous);
	return p->previous;
}

static bool consumes(vm_t* vm, parser_t* p, token_type_t t)
{
	if (peek(p) != t)
		return false;
	consume(vm, p);
	return true;
}

static bool must_consume(vm_t* vm, parser_t* p, token_type_t t, const char* error)
{
	bool res = false;
	if (!(res = consumes(vm, p, t))) {
		parse_error(vm, p, error);
	}
	return res;
}

static const grammar_rule_t* grammar_rule(token_type_t type);
static ast_node_t* parse_precedence(vm_t* vm, parser_t* p, enum precedence prec)
{
	const grammar_rule_t* prefix = grammar_rule(peek(p));
	if (!prefix->prefix) {
		parse_error(vm, p, "expected expression");
		lexer_dump_token(&p->lexer, &p->current);
		return NULL;
	}

	ast_node_t* node = prefix->prefix(vm, p);
	while (node != NULL) {
		const grammar_rule_t* infix = grammar_rule(peek(p));
		if (!infix->infix || infix->prec < prec || (infix->prec == prec && infix->assoc == ASSOC_LEFT))
			break;
		ast_node_t* new_node = infix->infix(vm, p, node);
		if (!new_node)
			free_node(node);
		node = new_node;
	}

	return node;
}

static ast_node_t* expression(vm_t* vm, parser_t* p)
{
	return parse_precedence(vm, p, PREC_LOWEST);
}

static ast_node_t* declaration(vm_t* vm, parser_t* p);
static ast_node_t* block_statement(vm_t* vm, parser_t* p, buffer_t* parameters)
{
	EXPECT(LEFT_BRACE, "'{' before block statement");

	scope_t* last_scope = p->scope;
	ast_node_t* node = make_block(p, parameters);
	while (peek(p) != TOKEN_EOF && peek(p) != TOKEN_RIGHT_BRACE) {
		ast_node_t* stmt = declaration(vm, p);
		if (!stmt) { free_node(node); node = NULL; break; }
		buffer_push(&node->block.body, &stmt);
	}
	p->scope = last_scope;

	if (!node) return NULL;

	EXPECT(RIGHT_BRACE, "'}' after block statement");
	return node;
}

static ast_node_t* if_statement(vm_t* vm, parser_t* p)
{
	EXPECT(IF, "'if'");

	ast_node_t* cnd = expression(vm, p);
	if (!cnd) goto fail;

	ast_node_t* csq = block_statement(vm, p, NULL);
	if (!csq) goto fail;

	ast_node_t* alt = NULL;
	if (consumes(vm, p, TOKEN_ELSE)) {
		alt = block_statement(vm, p, NULL);
		if (!alt) goto fail;
	}

	return make_branch(cnd, csq, alt);

fail:
	if (cnd) free_node(cnd);
	if (csq) free_node(csq);
	if (alt) free_node(alt);
	return NULL;
}

static ast_node_t* return_statement(vm_t* vm, parser_t* p)
{
	EXPECT(RETURN, "'return'");

	ast_node_t* expr = expression(vm, p);
	// if (!expr) return NULL;

	return make_return(expr);
}

static ast_node_t* statement(vm_t* vm, parser_t* p)
{
	switch (peek(p)) {
		case TOKEN_LEFT_BRACE: return block_statement(vm, p, NULL);
		case TOKEN_IF:         return if_statement(vm, p);
		case TOKEN_RETURN:     return return_statement(vm, p);
		default: return expression(vm, p);
	}
}

static ast_node_t* var_declaration(vm_t* vm, parser_t* p)
{
	EXPECT(VAR, "'var'");
	EXPECT(IDENTIFIER, "identifier after 'var'");

	token_t id = p->previous;
	if (scope_add_local(p->scope, &id) == NOT_FOUND) {
		parse_error(vm, p, "variable '%s' already declared", ((identifier_t*)buffer_at(&p->lexer.identifiers, id.index))->name);
		return NULL;
	}

	if (!must_consume(vm, p, TOKEN_EQUALS, "variable must be initialized")) return NULL;

	ast_node_t* init = expression(vm, p);
	if (!init) return NULL;

	return make_var_decl(id, init);
}

static ast_node_t* declaration(vm_t* vm, parser_t* p)
{
	switch (peek(p)) {
		case TOKEN_VAR: return var_declaration(vm, p);
		default: return statement(vm, p);
	}
}

#include "parser/grammar_rules.c"

// -----------------------------------------------------------------------------

parser_t parse(vm_t* vm, const char* source, const char* module)
{
	parser_t parser;
	parser.lexer = lexer_new(source, module);
	parser.current = lexer_next(vm, &parser.lexer);
	parser.scope = NULL;
	parser.root = make_block(&parser, NULL);

	while (!consumes(vm, &parser, TOKEN_EOF)) {
		ast_node_t* node = declaration(vm, &parser);
		if (!node) goto fail;
		buffer_push(&parser.root->block.body, &node);
	}
	return parser;

fail:
	parser_free(&parser);
	return parser;
}

void parser_free(parser_t* parser)
{
	if (parser->root) {
		free_node(parser->root);
		parser->root = NULL;
	}
	lexer_free(&parser->lexer);
}

void parser_dump_node(parser_t* parser, ast_node_t* node, unsigned indent)
{
	for (unsigned i = 0; i < indent; ++i)
		printf("  ");

	if (!node) {
		printf("(null node)\n");
		return;
	}

	switch (node->type) {
	case AST_BINARY:
		printf("BINARY (%s)\n", token_name(node->binary.operator));
		parser_dump_node(parser, node->binary.lhs, indent + 1);
		parser_dump_node(parser, node->binary.rhs, indent + 1);
		break;
	case AST_BLOCK:
		printf("BLOCK (%zu) [", node->block.body.size);
		for (size_t i = 0; i < node->block.scope->locals.size; ++i) {
			token_t* t = buffer_at(&node->block.scope->locals, i);
			identifier_t* id = buffer_at(&parser->lexer.identifiers, t->index);
			printf("%s%s", i > 0 ? ", " : "", id->name);
		}
		printf("] [");
		for (size_t i = 0; i < node->block.scope->upvalues.size; ++i) {
			token_t* t = buffer_at(&node->block.scope->upvalues, i);
			identifier_t* id = buffer_at(&parser->lexer.identifiers, t->index);
			printf("%s%s", i > 0 ? ", " : "", id->name);
		}
		printf("]\n");
		for (size_t i = 0; i < node->block.body.size; ++i) {
			parser_dump_node(parser, *((ast_node_t**)buffer_at(&node->block.body, i)), indent + 1);
		}
		break;
	case AST_BRANCH:
		printf("BRANCH\n");
		parser_dump_node(parser, node->branch.condition, indent + 1);
		parser_dump_node(parser, node->branch.consequent, indent + 1);
		parser_dump_node(parser, node->branch.alternate, indent + 1);
		break;
	case AST_CALL:
		printf("CALL\n");
		parser_dump_node(parser, node->call.callee, indent + 1);
		for (size_t i = 0; i < node->call.arguments.size; ++i)
			parser_dump_node(parser, *((ast_node_t**)buffer_at(&node->call.arguments, i)), indent + 1);
		break;
	case AST_FUNCTION:
		printf("FUNCTION (");
		for (size_t i = 0; i < node->function.parameters.size; ++i)
			printf("%s%s", i > 0 ? ", " : "", ((identifier_t*)buffer_at(&parser->lexer.identifiers, ((token_t*)buffer_at(&node->function.parameters, i))->index))->name);
		printf(")\n");
		parser_dump_node(parser, node->function.body, indent + 1);
		break;
	case AST_IDENTIFIER:
		printf("IDENTIFIER %s\n", node->identifier.id->name);
		break;
	case AST_LITERAL:
		if (node->literal.type == TOKEN_STRING)
			printf("LITERAL \"%.*s\"\n", (int)node->literal.lit->string.length, node->literal.lit->string.start);
		else
			printf("LITERAL %g\n", node->literal.lit->number);
		break;
	case AST_PROPERTY:
		printf("PROPERTY (%s) %s\n", token_name(node->property.operator), node->property.name->name);
		parser_dump_node(parser, node->property.lhs, indent + 1);
		break;
	case AST_RETURN:
		printf("RETURN\n");
		parser_dump_node(parser, node->ret.expression, indent + 1);
		break;
	case AST_UNARY:
		printf("UNARY (%s)\n", token_name(node->unary.operator));
		parser_dump_node(parser, node->unary.lhs, indent + 1);
		break;
	case AST_VAR_DECL:
		printf("VAR_DECL %s\n", ((identifier_t*)buffer_at(&parser->lexer.identifiers, node->var.identifier.index))->name);
		parser_dump_node(parser, node->var.initializer, indent + 1);
		break;
	default:
		printf("(unknown node)\n");
		break;
	}
}
