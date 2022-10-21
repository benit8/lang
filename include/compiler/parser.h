#pragma once

#include "buffer.h"
#include "compiler/lexer.h"
#include "vm.h"

#define UPVALUE_MASK 0x7000000000000000
#define NOT_FOUND    0x8000000000000000

typedef enum ast_type {
	AST_BINARY,
	AST_BLOCK,
	AST_BRANCH,
	AST_CALL,
	AST_FUNCTION,
	AST_IDENTIFIER,
	AST_LITERAL,
	AST_PROPERTY,
	AST_RETURN,
	AST_UNARY,
	AST_VAR_DECL,
} ast_type_t;

typedef struct scope {
	struct scope* parent;
	buffer_t locals;
	buffer_t upvalues;
} scope_t;

typedef struct ast_node {
	ast_type_t type;

	union {
		struct {
			token_type_t operator;
			struct ast_node* lhs, *rhs;
		} binary;
		struct {
			buffer_t body;
			scope_t* scope;
		} block;
		struct {
			struct ast_node* condition, *consequent, *alternate;
		} branch;
		struct {
			struct ast_node* callee;
			buffer_t arguments;
		} call;
		struct {
			buffer_t parameters;
			struct ast_node* body;
		} function;
		struct {
			token_t token;
			identifier_t* id;
		} identifier;
		struct {
			token_type_t type;
			literal_t* lit;
		} literal;
		struct {
			token_type_t operator;
			struct ast_node* lhs;
			identifier_t* name;
		} property;
		struct {
			struct ast_node* expression;
		} ret;
		struct {
			token_type_t operator;
			struct ast_node* lhs;
		} unary;
		struct {
			token_t identifier;
			struct ast_node* initializer;
		} var;
	};
} ast_node_t;

typedef struct parser {
	lexer_t lexer;
	token_t previous, current;
	scope_t* scope;
	ast_node_t* root;
} parser_t;

enum precedence {
	PREC_LOWEST,
	PREC_ASSIGNS,     // = += -= *= **= /= |= &= ^= <<= >>= ??=
	PREC_TERNARY,     // ?
	PREC_COALESCE,    // ?? ?:
	PREC_BOOLEAN_OR,  // ||
	PREC_BOOLEAN_AND, // &&
	PREC_BITWISE_OR,  // |
	PREC_BITWISE_XOR, // ^
	PREC_BITWISE_AND, // &
	PREC_EQUALITIES,  // == !=
	PREC_COMPARISONS, // < <= > >=
	PREC_SHIFTS,      // << >>
	PREC_RANGE,       // .. ...
	PREC_TERMS,       // + -
	PREC_FACTORS,     // * / %
	PREC_POWER,       // **
	PREC_UNARIES,     // ! + - ~
	PREC_UPDATES,     // ++ --
	PREC_PROPERTIES,  // . ?. :: \ ( [
};

enum associtivity {
	ASSOC_LEFT,
	ASSOC_RIGHT,
};

typedef struct grammar_rule {
	enum precedence prec;
	enum associtivity assoc;
	ast_node_t* (*prefix)(vm_t* vm, parser_t* p);
	ast_node_t* (*infix)(vm_t* vm, parser_t* p, ast_node_t* lhs);
} grammar_rule_t;

parser_t parse(vm_t* vm, const char* source, const char* module);
void parser_free(parser_t* parser);
void parser_dump_node(parser_t* parser, ast_node_t* node, unsigned indent);
