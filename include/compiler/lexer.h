#pragma once

#include <stddef.h>
#include "buffer.h"
#include "compiler/token_types.h"
#include "vm.h"

typedef enum token_type {
#define __ENUMERATE(type) TOKEN_ ## type,
	__ENUMERATE_TOKEN_TYPES
#undef __ENUMERATE
} token_type_t;

typedef struct token {
	token_type_t type;
	unsigned line, column;
	size_t index;
} token_t;

typedef union literal {
	struct {
		const char* start;
		size_t length;
	} string;
	double number;
} literal_t;

typedef struct identifier {
	char* name;
	unsigned reference_count;
} identifier_t;

typedef struct lexer {
	const char* source;
	const char* module;

	const char* current;
	unsigned line, column;

	buffer_t literals;
	buffer_t identifiers;
} lexer_t;

lexer_t lexer_new(const char* source, const char* module);
token_t lexer_next(vm_t* vm, lexer_t* lexer);
void lexer_free(lexer_t* lexer);
void lexer_dump_token(lexer_t* lexer, token_t* t);
const char* token_name(token_type_t t);
