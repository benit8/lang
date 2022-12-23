#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler/lexer.h"

#define TOKEN(type) ((token_t){type, l->line, l->column, 0})

static const char* token_type_names[] = {
#define __ENUMERATE(type) #type,
	__ENUMERATE_TOKEN_TYPES
#undef __ENUMERATE
};

static const struct {
	char* name;
	size_t length;
	token_type_t type;
} keywords[] = {
	{"else",   4, TOKEN_ELSE},
	{"false",  5, TOKEN_FALSE},
	{"for",    3, TOKEN_FOR},
	{"fn",     2, TOKEN_FUNCTION},
	{"if",     2, TOKEN_IF},
	{"match",  5, TOKEN_MATCH},
	{"null",   4, TOKEN_NULL},
	{"return", 6, TOKEN_RETURN},
	{"var",    3, TOKEN_VAR},
	{"while",  5, TOKEN_WHILE},
	{NULL,     0, TOKEN_UNKNOWN},
};

static void lex_error(vm_t* vm, const char* module, unsigned line, unsigned column, const char* message)
{
	char buf[128] = { 0 };
	snprintf(buf, 128, "Lex error in %s at %u,%u: %s", module, line, column, message);
	vm->error_handler(buf);
}

static inline char peek(lexer_t* l)
{
	return *l->current;
}

static void advance(lexer_t* l)
{
	if (peek(l) == '\n') {
		l->line++;
		l->column = 1;
	} else {
		l->column++;
	}
	l->current++;
}

static bool match(lexer_t* l, char c)
{
	if (peek(l) == c) {
		advance(l);
		return true;
	}
	return false;
}

static bool match2(lexer_t* l, char c1, char c2)
{
	if (peek(l) != '\0' && l->current[0] == c1 && l->current[1] == c2) {
		advance(l);
		advance(l);
		return true;
	}
	return false;
}

static inline bool fast_strncmp(const char* a, const char* b, size_t sa, size_t sb) {
	if (sa != sb)
		return false;
	return strncmp(a, b, sa < sb ? sa : sb) == 0;
}

static token_t identifier(lexer_t* l)
{
	const char* start = l->current;
	token_t t = TOKEN(TOKEN_IDENTIFIER);
	while (isalnum(l->current[1]) || l->current[1] == '_')
		advance(l);

	// look for a keyword
	size_t length = l->current - start + 1;
	advance(l);
	for (size_t i = 0; keywords[i].name != NULL; ++i) {
		if (fast_strncmp(start, keywords[i].name, length, keywords[i].length)) {
			t.type = keywords[i].type;
			return t;
		}
	}

	// look for a referenced identifier
	for (size_t i = 0; i < l->identifiers.size; ++i) {
		identifier_t* id = buffer_at(&l->identifiers, i);
		if (fast_strncmp(start, id->name, length, strlen(id->name))) {
			id->reference_count++;
			t.index = i;
			return t;
		}
	}

	// insert a new identifier
	identifier_t id = { ALLOC(length + 1), 1 };
	strncpy(id.name, start, length);
	buffer_push(&l->identifiers, &id);
	t.index = l->identifiers.size - 1;
	return t;
}

static token_t number(lexer_t* l)
{
	char* end = (char*)l->current;
	token_t t = TOKEN(TOKEN_NUMBER);
	literal_t lit = { .number = strtod(l->current, &end) };
	l->current = end;
	buffer_push(&l->literals, &lit);
	t.index = l->literals.size - 1;
	return t;
}

static token_t string(vm_t* vm, lexer_t* l)
{
	// Skip the quote
	advance(l);

	const char* start = l->current;
	token_t t = TOKEN(TOKEN_STRING);
	while (peek(l) != '\0' && peek(l) != '"')
		advance(l);

	if (peek(l) == '\0') {
		lex_error(vm, l->module, t.line, t.column, "Unterminated string");
		return TOKEN(TOKEN_UNKNOWN);
	}

	literal_t lit = { .string = { .start = start, .length = l->current - start } };
	buffer_push(&l->literals, &lit);
	t.index = l->literals.size - 1;

	// Skip the closing quote
	advance(l);

	return t;
}

static token_type_t operator(lexer_t* l)
{
	if (match(l, '!')) {
		if (match(l, '='))
			return TOKEN_EXCLAMATION_EQUALS;
		return TOKEN_EXCLAMATION;
	}
	if (match(l, '%')) {
		if (match(l, '='))
			return TOKEN_PERCENT_EQUALS;
		return TOKEN_PERCENT;
	}
	if (match(l, '&')) {
		if (match(l, '='))
			return TOKEN_AMPERSAND_EQUALS;
		else if (match(l, '&')) {
			if (match(l, '='))
				return TOKEN_AMPERSAND_AMPERSAND_EQUALS;
			return TOKEN_AMPERSAND_AMPERSAND;
		}
		return TOKEN_AMPERSAND;
	}
	if (match(l, '(')) {
		return TOKEN_LEFT_PARENTHESIS;
	}
	if (match(l, ')')) {
		return TOKEN_RIGHT_PARENTHESIS;
	}
	if (match(l, '*')) {
		if (match(l, '='))
			return TOKEN_ASTERISK_EQUALS;
		else if (match(l, '&')) {
			if (match(l, '='))
				return TOKEN_ASTERISK_ASTERISK_EQUALS;
			return TOKEN_ASTERISK_ASTERISK;
		}
		return TOKEN_ASTERISK;
	}
	if (match(l, '+')) {
		if (match(l, '+'))
			return TOKEN_PLUS_PLUS;
		else if (match(l, '='))
			return TOKEN_PLUS_EQUALS;
		return TOKEN_PLUS;
	}
	if (match(l, ',')) {
		return TOKEN_COMMA;
	}
	if (match(l, '-')) {
		if (match(l, '-'))
			return TOKEN_MINUS_MINUS;
		else if (match(l, '='))
			return TOKEN_MINUS_EQUALS;
		return TOKEN_MINUS;
	}
	if (match(l, '.')) {
		if (match(l, '.')) {
			if (match(l, '.'))
				return TOKEN_DOT_DOT_DOT;
			return TOKEN_DOT_DOT;
		}
		return TOKEN_DOT;
	}
	if (match(l, '/')) {
		if (match(l, '='))
			return TOKEN_SLASH_EQUALS;
		return TOKEN_SLASH;
	}
	if (match(l, ':')) {
		return TOKEN_COLON;
	}
	if (match(l, ';')) {
		return TOKEN_SEMICOLON;
	}
	if (match(l, '<')) {
		if (match(l, '=')) {
			if (match(l, '>'))
				return TOKEN_LESS_EQUALS_GREATER;
			return TOKEN_LESS_EQUALS;
		} else if (match(l, '<')) {
			if (match(l, '='))
				return TOKEN_LESS_LESS_EQUALS;
			return TOKEN_LESS_LESS;
		}
		return TOKEN_LESS;
	}
	if (match(l, '=')) {
		if (match(l, '='))
			return TOKEN_EQUALS_EQUALS;
		else if (match(l, '>'))
			return TOKEN_EQUALS_GREATER;
		return TOKEN_EQUALS;
	}
	if (match(l, '>')) {
		if (match(l, '='))
			return TOKEN_GREATER_EQUALS;
		else if (match(l, '>')) {
			if (match(l, '='))
				return TOKEN_GREATER_GREATER_EQUALS;
			return TOKEN_GREATER_GREATER;
		}
		return TOKEN_GREATER;
	}
	if (match(l, '?')) {
		if (match(l, '.'))
			return TOKEN_QUESTION_DOT;
		else if (match(l, '?')) {
			if (match(l, '='))
				return TOKEN_QUESTION_QUESTION_EQUALS;
			return TOKEN_QUESTION_QUESTION;
		}
		return TOKEN_QUESTION;
	}
	if (match(l, '[')) {
		return TOKEN_LEFT_BRACKET;
	}
	if (match(l, ']')) {
		return TOKEN_RIGHT_BRACKET;
	}
	if (match(l, '^')) {
		if (match(l, '='))
			return TOKEN_CARET_EQUALS;
		return TOKEN_CARET;
	}
	if (match(l, '{')) {
		return TOKEN_LEFT_BRACE;
	}
	if (match(l, '|')) {
		if (match(l, '='))
			return TOKEN_PIPE_EQUALS;
		else if (match(l, '|')) {
			if (match(l, '='))
				return TOKEN_PIPE_PIPE_EQUALS;
			return TOKEN_PIPE_PIPE;
		}
		return TOKEN_PIPE;
	}
	if (match(l, '}')) {
		return TOKEN_RIGHT_BRACE;
	}
	return TOKEN_UNKNOWN;
}

// -----------------------------------------------------------------------------

lexer_t lexer_new(const char* source, const char* module)
{
	return (lexer_t) {
		.source = source,
		.module = module,
		.current = source,
		.column = 1,
		.line = 1,
		.literals = buffer_new(sizeof(literal_t)),
		.identifiers = buffer_new(sizeof(identifier_t)),
	};
}

token_t lexer_next(vm_t* vm, lexer_t* l)
{
	while (true) {
		// Ignore whitespaces
		while (isspace(peek(l))) advance(l);
		// Skip line comment
		if (match2(l, '/', '/'))
			while (!match(l, '\n')) advance(l);
		// Skip block comment
		else if (match2(l, '/', '*'))
			while (!match2(l, '*', '/')) advance(l);
		else
			break;
	}

	if (peek(l) == '\0') {
		return TOKEN(TOKEN_EOF);
	}

	if (isalpha(peek(l)) || peek(l) == '_') {
		return identifier(l);
	}

	if (isdigit(peek(l)) || (peek(l) == '.' && isdigit(l->current[1]))) {
		return number(l);
	}

	if (peek(l) == '"') {
		return string(vm, l);
	}

	if (ispunct(peek(l))) {
		return TOKEN(operator(l));
	}

	lex_error(vm, l->module, l->line, l->column, "Unknown character");

	token_t t = TOKEN(TOKEN_UNKNOWN);
	advance(l);
	return t;
}

void lexer_free(lexer_t* lexer)
{
	lexer->source = NULL;
	lexer->module = NULL;

	buffer_foreach(lexer->identifiers, identifier_t, id) {
		FREE(id->name);
	}

	buffer_free(&lexer->literals);
	buffer_free(&lexer->identifiers);
}

void lexer_dump_token(lexer_t* lexer, token_t* t)
{
	printf("%s %u,%u", token_type_names[t->type], t->line, t->column);
	if (t->type == TOKEN_IDENTIFIER) {
		identifier_t* id = buffer_at(&lexer->identifiers, t->index);
		printf(" %s", id->name);
	}
	else if (t->type == TOKEN_NUMBER) {
		literal_t* lit = buffer_at(&lexer->literals, t->index);
		printf(" %g", lit->number);
	}
	else if (t->type == TOKEN_STRING) {
		literal_t* lit = buffer_at(&lexer->literals, t->index);
		printf(" %.*s", (int)lit->string.length, lit->string.start);
	}
	printf("\n");
}

const char* token_name(token_type_t t)
{
	return token_type_names[t];
}
