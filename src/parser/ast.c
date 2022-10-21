
static ast_node_t* make_binary(token_type_t op, ast_node_t* lhs, ast_node_t* rhs)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_BINARY;
	node->binary.operator = op;
	node->binary.lhs = lhs;
	node->binary.rhs = rhs;
	return node;
}

static ast_node_t* make_block(parser_t* p, buffer_t* parameters)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_BLOCK;
	node->block.body = buffer_new(sizeof(ast_node_t*));
	node->block.scope = ALLOC(sizeof(scope_t));
	node->block.scope->parent = p->scope;
	node->block.scope->locals = buffer_new(sizeof(token_t));
	node->block.scope->upvalues = buffer_new(sizeof(token_t));
	p->scope = node->block.scope;
	if (parameters) {
		for (size_t i = 0; i < parameters->size; ++i) {
			scope_add_local(node->block.scope, buffer_at(parameters, i));
		}
	}
	return node;
}

static ast_node_t* make_branch(ast_node_t* cnd, ast_node_t* csq, ast_node_t* alt)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_BRANCH;
	node->branch.condition = cnd;
	node->branch.consequent = csq;
	node->branch.alternate = alt;
	return node;
}

static ast_node_t* make_call(ast_node_t* callee, buffer_t args)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_CALL;
	node->call.callee = callee;
	node->call.arguments = args;
	return node;
}

static ast_node_t* make_function(buffer_t params, ast_node_t* body)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_FUNCTION;
	node->function.parameters = params;
	node->function.body = body;
	return node;
}

static ast_node_t* make_identifier(token_t t, identifier_t* id)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_IDENTIFIER;
	node->identifier.token = t;
	node->identifier.id = id;
	return node;
}

static ast_node_t* make_literal(token_type_t type, literal_t* lit)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_LITERAL;
	node->literal.type = type;
	node->literal.lit = lit;
	return node;
}

static ast_node_t* make_property(token_type_t op, ast_node_t* lhs, identifier_t* name)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_PROPERTY;
	node->property.operator = op;
	node->property.lhs = lhs;
	node->property.name = name;
	return node;
}

static ast_node_t* make_return(ast_node_t* expr)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_RETURN;
	node->ret.expression = expr;
	return node;
}

static ast_node_t* make_var_decl(token_t id, ast_node_t* init)
{
	ast_node_t* node = ALLOC(sizeof(ast_node_t));
	node->type = AST_VAR_DECL;
	node->var.identifier = id;
	node->var.initializer = init;
	return node;
}
