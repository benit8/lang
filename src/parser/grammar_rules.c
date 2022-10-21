
static bool parse_parameter_list(vm_t* vm, parser_t* p, buffer_t* parameters)
{
	while (!consumes(vm, p, TOKEN_RIGHT_PARENTHESIS)) {
		if (parameters->size > 0 && !must_consume(vm, p, TOKEN_COMMA, "comma must separate parameters")) return false;

		token_t t = p->current;
		if (!must_consume(vm, p, TOKEN_IDENTIFIER, "expected parameter")) return false;
		buffer_push(parameters, &t);
	}

	return true;
}

static bool parse_argument_list(vm_t* vm, parser_t* p, buffer_t* arguments)
{
	while (!consumes(vm, p, TOKEN_RIGHT_PARENTHESIS)) {
		if (arguments->size > 0 && !must_consume(vm, p, TOKEN_COMMA, "comma must separate arguments")) return false;

		ast_node_t* arg = expression(vm, p);
		if (!arg) return false;
		buffer_push(arguments, &arg);
	}

	return true;
}

static ast_node_t* gr_binary(vm_t* vm, parser_t* p, ast_node_t* lhs)
{
	token_type_t op = consume(vm, p).type;

	ast_node_t* rhs = parse_precedence(vm, p, grammar_rule(op)->prec);
	if (!rhs) return NULL;

	return make_binary(op, lhs, rhs);
}

static ast_node_t* gr_call(vm_t* vm, parser_t* p, ast_node_t* lhs)
{
	EXPECT(LEFT_PARENTHESIS, "argument list after '('");

	buffer_t args = buffer_new(sizeof(ast_node_t*));
	if (!parse_argument_list(vm, p, &args)) goto fail;

	return make_call(lhs, args);

fail:
	buffer_free(&args);
	return NULL;
}

static ast_node_t* gr_function(vm_t* vm, parser_t* p)
{
	// Skip 'fn'
	EXPECT(FUNCTION, "fn");
	EXPECT(LEFT_PARENTHESIS, "parameter list after 'fn'");

	buffer_t params = buffer_new(sizeof(token_t));
	if (!parse_parameter_list(vm, p, &params)) goto fail;

	ast_node_t* body = NULL;
	if (consumes(vm, p, TOKEN_EQUALS_GREATER)) {
		// Still need to make a block to insert parameters as locals and upvalues
		body = make_block(p, &params);
		ast_node_t* ret = make_return(expression(vm, p));
		if (!ret->ret.expression) { FREE(ret); goto fail; }
		buffer_push(&body->block.body, &ret);
		p->scope = body->block.scope->parent;
	} else {
		body = block_statement(vm, p, &params);
		if (!body) goto fail;
	}

	return make_function(params, body);

fail:
	buffer_free(&params);
	if (body) free_node(body);
	return NULL;
}

static ast_node_t* gr_identifier(vm_t* vm, parser_t* p)
{
	token_t name = consume(vm, p);
	identifier_t* id = buffer_at(&p->lexer.identifiers, name.index);

	if (scope_find_local_or_upvalue(p->scope, &name) == NOT_FOUND) {
		// parse_error(vm, p, "undefined variable '%s'", id->name);
		// return NULL;
	}

	return make_identifier(name, id);
}

/*static ast_node_t* gr_index(vm_t* vm, parser_t* p, ast_node_t* lhs)
{
	EXPECT(LEFT_BRACKET, "'['");

	ast_node_t* index = expression(vm, p);
	if (!index) return NULL;

	if (!must_consume(vm, p, TOKEN_RIGHT_BRACKET, "expected ']'")) {
		free_node(index);
		return NULL;
	}

	// TODO: compile into a call to "lhs.__at(rhs)"
	assert(false);
	return NULL;
}*/

static ast_node_t* gr_literal(vm_t* vm, parser_t* p)
{
	token_t t = consume(vm, p);
	return make_literal(
		t.type,
		(t.type == TOKEN_STRING || t.type == TOKEN_NUMBER) ? buffer_at(&p->lexer.literals, t.index) : NULL
	);
}

static ast_node_t* gr_property(vm_t* vm, parser_t* p, ast_node_t* lhs)
{
	token_type_t op = consume(vm, p).type;

	EXPECT(IDENTIFIER, "identifier after '.'");
	token_t property = p->previous;

	return make_property(op, lhs, buffer_at(&p->lexer.identifiers, property.index));
}

static ast_node_t* gr_ternary(vm_t* vm, parser_t* p, ast_node_t* condition)
{
	EXPECT(QUESTION, "'?'");

	ast_node_t* lhs = parse_precedence(vm, p, PREC_TERNARY);
	if (!lhs) goto fail;

	if (!must_consume(vm, p, TOKEN_COLON, "expected ':' in ternary expression")) goto fail;

	ast_node_t* rhs = parse_precedence(vm, p, PREC_TERNARY);
	if (!rhs) goto fail;

	return make_branch(condition, lhs, rhs);

fail:
	if (lhs) free_node(lhs);
	if (rhs) free_node(rhs);
	return NULL;
}

static const grammar_rule_t grammar_rules[] = {
	/* AMPERSAND                  */ { PREC_BITWISE_AND, ASSOC_LEFT,  NULL, gr_binary }, // __band
	/* AMPERSAND_AMPERSAND        */ { PREC_BOOLEAN_AND, ASSOC_LEFT,  NULL, gr_binary }, // __and
	/* AMPERSAND_AMPERSAND_EQUALS */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* AMPERSAND_EQUALS           */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* ASTERISK                   */ { PREC_FACTORS,     ASSOC_LEFT,  NULL, gr_binary }, // __mul
	/* ASTERISK_ASTERISK          */ { PREC_POWER,       ASSOC_RIGHT, NULL, gr_binary }, // __pow
	/* ASTERISK_ASTERISK_EQUALS   */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* ASTERISK_EQUALS            */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* BACKSLASH                  */ { PREC_LOWEST,      ASSOC_LEFT,  NULL, NULL },
	/* CARET                      */ { PREC_BITWISE_XOR, ASSOC_LEFT,  NULL, gr_binary }, // __xor
	/* CARET_EQUALS               */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* COLON                      */ { 0, 0, NULL, NULL },
	/* COLON_COLON                */ { 0, 0, NULL, NULL },
	/* COMMA                      */ { 0, 0, NULL, NULL },
	/* DEFAULT                    */ { 0, 0, NULL, NULL },
	/* DOT                        */ { PREC_PROPERTIES,  ASSOC_LEFT,  NULL, gr_property }, // __get
	/* DOT_DOT                    */ { PREC_RANGE,       ASSOC_LEFT,  NULL, NULL }, // __erange
	/* DOT_DOT_DOT                */ { PREC_RANGE,       ASSOC_LEFT,  NULL, NULL }, // __irange
	/* ELSE                       */ { 0, 0, NULL, NULL },
	/* EOF                        */ { 0, 0, NULL, NULL },
	/* EQUALS                     */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* EQUALS_EQUALS              */ { PREC_EQUALITIES,  ASSOC_LEFT,  NULL, gr_binary }, // __eq
	/* EQUALS_GREATER             */ { 0, 0, NULL, NULL },
	/* EXCLAMATION                */ { PREC_UNARIES,     ASSOC_RIGHT, NULL, NULL }, // __not
	/* EXCLAMATION_EQUALS         */ { PREC_EQUALITIES,  ASSOC_LEFT,  NULL, gr_binary }, // __neq
	/* FALSE                      */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* FOR                        */ { 0, 0, NULL, NULL },
	/* FUNCTION                   */ { PREC_LOWEST,      ASSOC_RIGHT, gr_function, NULL },
	/* GLYPH                      */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* GREATER                    */ { PREC_COMPARISONS, ASSOC_LEFT,  NULL, gr_binary }, // __gt
	/* GREATER_EQUALS             */ { PREC_COMPARISONS, ASSOC_LEFT,  NULL, gr_binary }, // __gte
	/* GREATER_GREATER            */ { PREC_SHIFTS,      ASSOC_LEFT,  NULL, gr_binary }, // __rshift
	/* GREATER_GREATER_EQUALS     */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* IDENTIFIER                 */ { PREC_LOWEST,      ASSOC_LEFT,  gr_identifier, NULL },
	/* IF                         */ { 0, 0, NULL, NULL },
	/* LEFT_BRACE                 */ { PREC_LOWEST,      ASSOC_RIGHT, NULL, NULL },
	/* LEFT_BRACKET               */ { PREC_PROPERTIES,  ASSOC_LEFT,  NULL, NULL/*gr_index*/ }, // __at
	/* LEFT_PARENTHESIS           */ { PREC_PROPERTIES,  ASSOC_LEFT,  NULL, gr_call }, // __call
	/* LESS                       */ { PREC_COMPARISONS, ASSOC_LEFT,  NULL, gr_binary }, // __lt
	/* LESS_EQUALS                */ { PREC_COMPARISONS, ASSOC_LEFT,  NULL, gr_binary }, // __lte
	/* LESS_EQUALS_GREATER        */ { PREC_COMPARISONS, ASSOC_LEFT,  NULL, gr_binary }, // __cmp
	/* LESS_LESS                  */ { PREC_SHIFTS,      ASSOC_LEFT,  NULL, gr_binary }, // __lshift
	/* LESS_LESS_EQUALS           */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* MATCH                      */ { PREC_LOWEST,      ASSOC_RIGHT, NULL, NULL },
	/* MINUS                      */ { PREC_TERMS,       ASSOC_LEFT,  NULL, gr_binary }, // __sub, __inv
	/* MINUS_EQUALS               */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* MINUS_MINUS                */ { PREC_UPDATES,     ASSOC_RIGHT, NULL, NULL }, // __dec
	/* NULL                       */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* NUMBER                     */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* PERCENT                    */ { PREC_FACTORS,     ASSOC_LEFT,  NULL, gr_binary }, // __mod
	/* PERCENT_EQUALS             */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* PIPE                       */ { PREC_BITWISE_OR,  ASSOC_LEFT,  NULL, gr_binary }, // __bor
	/* PIPE_EQUALS                */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* PIPE_PIPE                  */ { PREC_BOOLEAN_OR,  ASSOC_LEFT,  NULL, gr_binary }, // __or
	/* PIPE_PIPE_EQUALS           */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* PLUS                       */ { PREC_TERMS,       ASSOC_LEFT,  NULL, gr_binary }, // __add
	/* PLUS_EQUALS                */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* PLUS_PLUS                  */ { PREC_UPDATES,     ASSOC_RIGHT, NULL, NULL }, // __inc
	/* QUESTION                   */ { PREC_TERNARY,     ASSOC_RIGHT, NULL, gr_ternary },
	/* QUESTION_COLON             */ { PREC_COALESCE,    ASSOC_LEFT,  NULL, gr_binary },
	/* QUESTION_DOT               */ { PREC_PROPERTIES,  ASSOC_LEFT,  NULL, gr_property }, // __get (nullsafe)
	/* QUESTION_QUESTION          */ { PREC_COALESCE,    ASSOC_LEFT,  NULL, NULL },
	/* QUESTION_QUESTION_EQUALS   */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* RETURN                     */ { 0, 0, NULL, NULL },
	/* RIGHT_BRACE                */ { 0, 0, NULL, NULL },
	/* RIGHT_BRACKET              */ { 0, 0, NULL, NULL },
	/* RIGHT_PARENTHESIS          */ { 0, 0, NULL, NULL },
	/* SEMICOLON                  */ { 0, 0, NULL, NULL },
	/* SLASH                      */ { PREC_FACTORS,     ASSOC_LEFT,  NULL, gr_binary }, // __div
	/* SLASH_EQUALS               */ { PREC_ASSIGNS,     ASSOC_RIGHT, NULL, NULL },
	/* STRING                     */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* TILDE                      */ { PREC_UNARIES,     ASSOC_RIGHT, NULL, NULL }, // __bnot
	/* TRUE                       */ { PREC_LOWEST,      ASSOC_RIGHT, gr_literal, NULL },
	/* UNKNOWN                    */ { 0, 0, NULL, NULL },
	/* VAR                        */ { 0, 0, NULL, NULL },
	/* WHILE                      */ { 0, 0, NULL, NULL },
};

static const grammar_rule_t* grammar_rule(token_type_t type)
{
	return &grammar_rules[type];
}
