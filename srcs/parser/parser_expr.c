#include "parser_internal.h"

ASTNode	*parse_expression(Parser *parser, Precedence precedence)
{
	if (parser->expr_depth >= MAX_EXPRESSION_DEPTH)
	{
		parser_error(parser, 
				"Expression too deeply nested (max %d)",
				MAX_EXPRESSION_DEPTH);
		return (NULL);
	}
	parser->expr_depth++;
	parser_advance(parser);
	ParsePrefixFn prefix = rules[parser->current.type].prefix;

	if (prefix == NULL)
	{
		parser_error(parser, "Expect expression");
		parser->expr_depth--;
		return (NULL);
	}

	ASTNode *left = prefix(parser);
	while (precedence < get_token_precedence(parser->next.type))
	{
		parser_advance(parser);
		ParseInfixFn infix = rules[parser->current.type].infix;
		if (infix)
			left = infix(parser, left);
	}
	parser->expr_depth--;
	return (left);
}

ASTNode	*parse_assignment(Parser *parser, ASTNode *left)
{
	if (left->type != AST_IDENTIFIER)
	{
		parser_error(parser, "Invalid assignment target");
		return (NULL);
	}
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_ASSIGNMENT;
	node->assignment.var_name = left->identifier.name;
	node->assignment.value = parse_expression(parser, PREC_ASSIGNMENT);
	return (node);
}

ASTNode	*parse_unary(Parser *parser)
{
	TokenType operator_type = parser->current.type;
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));

	switch (operator_type)
	{
		case TOKEN_MINUS:	node->type = AST_NEGATE; break;
		case TOKEN_BANG:	node->type = AST_NOT; break;
		case TOKEN_BIT_NOT:	node->type = AST_BIT_NOT; break;
		default:			node->type = AST_NUMBER; break;
	}

	node->unary.operand = parse_expression(parser, PREC_UNARY);
	return (node);
}

ASTNode	*parse_binary(Parser *parser, ASTNode *left)
{
	TokenType operator_type = parser->current.type;
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));

	switch (operator_type)
	{
		case TOKEN_PLUS:			node->type = AST_ADD; break;
		case TOKEN_MINUS:			node->type = AST_SUB; break;
		case TOKEN_STAR:			node->type = AST_MUL; break;
		case TOKEN_SLASH:			node->type = AST_DIV; break;
		case TOKEN_EQUAL_EQUAL:		node->type = AST_EQUAL; break;
		case TOKEN_BANG_EQUAL:		node->type = AST_NOT_EQUAL; break;
		case TOKEN_LESS:			node->type = AST_LESS; break;
		case TOKEN_LESS_EQUAL:		node->type = AST_LESS_EQUAL; break;
		case TOKEN_GREATER:			node->type = AST_GREATER; break;
		case TOKEN_GREATER_EQUAL:	node->type = AST_GREATER_EQUAL; break;
		case TOKEN_LSHIFT:			node->type = AST_LSHIFT; break;
		case TOKEN_RSHIFT:			node->type = AST_RSHIFT; break;
		case TOKEN_BIT_AND:			node->type = AST_BIT_AND; break;
		case TOKEN_BIT_OR:			node->type = AST_BIT_OR; break;
		case TOKEN_BIT_XOR:			node->type = AST_BIT_XOR; break;
		default: 
			parser_error(parser, "invalid binary operator");
			return (NULL);
	}

	Precedence precedence = get_token_precedence(operator_type);
	node->binary.left = left;
	node->binary.right = parse_expression(parser, (Precedence)(precedence));
	return (node);
}

ASTNode	*parse_number(Parser *parser)
{
	ASTNode	*node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_NUMBER;
	node->number.value = parser->current.text;
	node->line = parser->current.line;
	return (node);
}

ASTNode	*parse_grouping(Parser *parser)
{
	ASTNode	*expression = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_RPAREN, "Expect ')' after expression.");
	return (expression);
}

ASTNode	*parse_identifier(Parser *parser)
{
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_IDENTIFIER;
	node->identifier.name = parser->current.text;
	node->line = parser->current.line;
	return (node);
}

ASTNode	*parse_call(Parser *parser, ASTNode *callee)
{
	if (callee->type != AST_IDENTIFIER)
	{
		parser_error(parser, "Can only call functions");
		return (NULL);
	}

	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_CALL;
	node->call.function_name = callee->identifier.name;
	node->line = callee->line;
	node->column = callee->column;

	ASTNode **args = arena_alloc(parser->arena, sizeof(ASTNode*) * MAX_PARAMS_PER_FUNCTION);
	size_t arg_count = 0;

	if (!check(parser, TOKEN_RPAREN))
	{
		do
		{
			if (arg_count >= MAX_PARAMS_PER_FUNCTION)
			{
				parser_error(parser, "Too many arguments (max %d)",
						MAX_PARAMS_PER_FUNCTION);
				parser_synchronize(parser);
				break;
			}
			ASTNode *arg = parse_expression(parser, PREC_ASSIGNMENT);
			if (!arg)
			{
				parser_synchronize(parser);
				break;
			}
			args[arg_count++] = arg;
		} while (match(parser, TOKEN_COMMA));
	}

	parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
	node->call.args = args;
	node->call.arg_count = arg_count;

	return (node);
}
