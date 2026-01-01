#include "ast.h"
#include "parser_internal.h"

ASTNode	*parse_statement(Parser *parser)
{
	TokenType type = parser->next.type;
	ParseStmtFn stmt_fn = rules[type].stmt;
	if (stmt_fn)
		return (stmt_fn(parser));

	return (parse_expr_stmt(parser));
}

ASTNode* parse_block(Parser *parser)
{
	parser_consume(parser, TOKEN_LBRACE, "Expected '{'");
	
	ASTNode **stmts = arena_alloc(
			parser->arena, sizeof(ASTNode*) * MAX_BLOCK_STATEMENTS);
	size_t count = 0;
	
	while (!check(parser, TOKEN_RBRACE) 
			&& !check(parser, TOKEN_EOF))
	{
		if (count >= MAX_BLOCK_STATEMENTS)
		{
			parser_error(parser, 
					"Block too large (max %d statements)",
					MAX_BLOCK_STATEMENTS);
			parser_synchronize(parser);
			break;
		}
		ASTNode *stmt = parse_statement(parser);
		if (stmt)
			stmts[count++] = stmt;
		else
			parser_synchronize(parser);
	}
	
	parser_consume(parser, TOKEN_RBRACE, "Expected '}'");
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_BLOCK,
		.block = {
			.statements = stmts,
			.count = count
		}
	};
	return (node);
}

ASTNode	*parse_function(Parser *parser)
{
	if (!match(parser, TOKEN_INT))
		return (NULL);

	DataType return_type = TYPE_INT64; // TODO Add support for other return types

	parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name.");
	StringView func_name = parser->current.text;
	int func_line = parser->current.line;

	parser_consume(parser, TOKEN_LPAREN, "Expected '(' after function name.");

	Parameter *params = NULL;
	size_t param_count = 0;

	if (!check(parser, TOKEN_RPAREN))
	{
		params = arena_alloc(parser->arena, sizeof(Parameter) * 16);
		if (check(parser, TOKEN_IDENTIFIER) && sv_eq_cstr(parser->next.text, "void"))
			parser_advance(parser);
		else
		{
			do
			{
				parser_consume(parser, TOKEN_INT, "Expected parameter type");
				parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
				params[param_count++] = (Parameter){
					.name = parser->current.text,
					.type = TYPE_INT64 // TODO
				};
			} while (match(parser, TOKEN_COMMA));
		}
	}

	parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters.");

	ASTNode *body = NULL;
	bool is_prototype = false;
	if (match(parser, TOKEN_SEMICOLON))
		is_prototype = true;
	else
		body = parse_block(parser);

	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_FUNCTION,
		.value_type = return_type,
		.line = func_line,
		.column = parser->current.column,
		.function = {
			.name = func_name,
			.return_type = return_type,
			.params = params,
			.param_count = param_count,
			.body = body,
			.is_prototype = is_prototype
		}
	};
	return (node);
}

ASTNode	*parse_var_decl(Parser *parser)
{
	parser_consume(parser, TOKEN_INT, "Expected 'int'");
	DataType var_type = TYPE_INT64; //TODO
	parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
	StringView var_name = parser->current.text;
	int var_line = parser->current.line;
	ASTNode *init = NULL;
	if (match(parser, TOKEN_EQUAL))
		init = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_VAR_DECL,
		.value_type = var_type,
		.line = var_line,
		.var_decl = { 
			.var_name = var_name, 
			.var_type = var_type,
			.initializer = init 
		}
	};
	return (node);
}

ASTNode	*parse_return(Parser *parser)
{
	parser_consume(parser, TOKEN_RETURN, "Expected 'return'");
	int ret_line = parser->current.line;
	ASTNode *expr = NULL;
	if (!check(parser, TOKEN_SEMICOLON))
		expr = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_RETURN,
		.value_type = TYPE_VOID,
		.line = ret_line,
		.return_stmt = { .expression = expr }
	};
	return (node);
}

ASTNode	*parse_if(Parser *parser)
{
	parser_consume(parser, TOKEN_IF, "Expected 'if'");
	int if_line = parser->current.line;
	parser_consume(parser, TOKEN_LPAREN, "Expected '('");
	ASTNode *condition = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_RPAREN, "Expected ')'");
	ASTNode *then_branch = parse_statement(parser);
	ASTNode *else_branch = NULL;
	if (match(parser, TOKEN_ELSE))
		else_branch = parse_statement(parser);
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_IF,
		.value_type = TYPE_VOID,
		.line = if_line,
		.if_stmt = { condition, then_branch, else_branch }
	};
	return (node);
}

ASTNode	*parse_while(Parser *parser)
{
	parser_consume(parser, TOKEN_WHILE, "Expected 'while'");
	int while_line = parser->current.line;
	parser_consume(parser, TOKEN_LPAREN, "Expected '('");
	ASTNode *condition = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_RPAREN, "Expected ')'");
	ASTNode *body = parse_statement(parser);
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_WHILE,
		.value_type = TYPE_VOID,
		.line = while_line,
		.while_stmt = { condition, body }
	};
	return (node);
}

ASTNode	*parse_expr_stmt(Parser *parser)
{
	ASTNode *expr = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	return (expr);
}
