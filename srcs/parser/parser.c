#include "parser.h"
#include "ast.h"
#include "defines.h"
#include "error_handler.h"
#include "lexer.h"
#include "string_view.h"
#include <stdio.h>

static ASTNode		*parse_var_decl(Parser *parser);
static ASTNode		*parse_return(Parser *parser);
static ASTNode		*parse_if(Parser *parser);
static ASTNode		*parse_while(Parser *parser);
static ASTNode		*parse_expr_stmt(Parser *parser);
static ASTNode		*parse_assignment(Parser *p, ASTNode *left);

static ASTNode		*parse_expression(Parser *parser, Precedence precedence);
static ASTNode		*parse_identifier(Parser *parser);
static ASTNode		*parse_grouping(Parser *parser);
static ASTNode		*parse_number(Parser *parser);
static ASTNode		*parse_unary(Parser *parser);
static ASTNode		*parse_binary(Parser *parser, ASTNode *left);
static ASTNode		*parse_statement(Parser *parser);
static ASTNode		*parse_block(Parser *parser);
static ASTNode		*parse_function(Parser *parser);
static ASTNode		*parse_call(Parser *parser, ASTNode *callee);
static void			parser_advance(Parser *parser);
static const char	*token_type_name(TokenType type);
static void 		parser_error(Parser *parser, const char *fmt, ...);

static bool inline	check(Parser *parser, TokenType type);
static bool inline	match(Parser *parser, TokenType type);

static void			parser_consume(Parser *parser, TokenType type, const char *message);
	
static const ParseRule rules[] = {
	#define X_TOKEN(name, str, is_keyword, prec, prefix, infix, stmt) {str, prec, prefix, infix, stmt},
	#include "lexer_tokens.def"
	#undef X_TOKEN
};

static ASTNode	*parse_statement(Parser *parser)
{
	TokenType type = parser->next.type;
	ParseStmtFn stmt_fn = rules[type].stmt;
	if (stmt_fn)
		return (stmt_fn(parser));

	return (parse_expr_stmt(parser));
}

static const char	*token_type_name(TokenType type)
{
	size_t rule_count = sizeof(rules) / sizeof(rules[0]);
	if (type < 0 || (size_t)type >= rule_count)
		return ("UNKNOWN");
	return (rules[type].name);
}

static Precedence	get_token_precedence(TokenType type)
{
	size_t rule_count = sizeof(rules) / sizeof(rules[0]);
	if (type < 0 || (size_t)type >= rule_count)
		return (PREC_NONE);
	return (rules[type].prec);
}

static ASTNode	*parse_expression(Parser *parser, Precedence precedence)
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

static ASTNode	*parse_assignment(Parser *parser, ASTNode *left)
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

static ASTNode	*parse_var_decl(Parser *parser)
{
	parser_consume(parser, TOKEN_INT, "Expected 'int'");
	parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name");
	StringView var_name = parser->current.text;
	ASTNode *init = NULL;
	if (match(parser, TOKEN_EQUAL))
		init = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_VAR_DECL,
		.var_decl = { .var_name = var_name, .initializer = init }
	};
	return (node);
}

static ASTNode	*parse_return(Parser *parser)
{
	parser_consume(parser, TOKEN_RETURN, "Expected 'return'");
	ASTNode *expr = NULL;
	if (!check(parser, TOKEN_SEMICOLON))
		expr = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_RETURN,
		.return_stmt = { .expression = expr }
	};
	return (node);
}

static ASTNode	*parse_if(Parser *parser)
{
	parser_consume(parser, TOKEN_IF, "Expected 'if'");
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
		.if_stmt = { condition, then_branch, else_branch }
	};
	return (node);
}

static ASTNode	*parse_while(Parser *parser)
{
	parser_consume(parser, TOKEN_WHILE, "Expected 'while'");
	parser_consume(parser, TOKEN_LPAREN, "Expected '('");
	ASTNode *condition = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_RPAREN, "Expected ')'");
	ASTNode *body = parse_statement(parser);
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_WHILE,
		.while_stmt = { condition, body }
	};
	return (node);
}

static ASTNode	*parse_expr_stmt(Parser *parser)
{
	ASTNode *expr = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_SEMICOLON, "Expected ';'");
	return (expr);
}

static void parser_error(Parser *parser, const char *fmt, ...)
{
	if (parser->panic_mode)
		return;
	parser->panic_mode = true;
	va_list	args;
	va_start(args, fmt);
	char	buffer[512];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	error_parser(parser->errors, parser->lexer->file->name,
			parser->current.line, parser->current.column,
			"%s", buffer);
}

static void	parser_synchronize(Parser *parser)
{
	parser->panic_mode = false;
	while (parser->next.type != TOKEN_EOF)
	{
		if (parser->current.type == TOKEN_SEMICOLON)
			return;
		switch (parser->next.type)
		{
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_RETURN:
			case TOKEN_INT:
			case TOKEN_LBRACE:
				return;
			default:
				break;
		}
		parser_advance(parser);
	}
}

static void	parser_advance(Parser *parser)
{
	parser->current = parser->next;
	while (true)
	{
		parser->next = lexer_next(parser->lexer);
		if (parser->next.type != TOKEN_ERROR)
			break;
	}
}

static void	parser_consume(Parser *parser, TokenType type, const char *message)
{
	if (parser->next.type == type)
	{
		parser_advance(parser);
		parser->panic_mode = false;
		return;
	}
	parser_error(parser, "%s (got %s)", message, 
			token_type_name(parser->next.type));
	parser_synchronize(parser);
}

static bool inline match(Parser *parser, TokenType type)
{
	if (parser->next.type == type)
	{
		parser_advance(parser);
		return (true);
	}
	return (false);
}

static bool inline check(Parser *parser, TokenType type)
{
	return (parser->next.type == type);
}

static ASTNode	*parse_call(Parser *parser, ASTNode *callee)
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

static ASTNode	*parse_identifier(Parser *parser)
{
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_IDENTIFIER;
	node->identifier.name = parser->current.text;
	node->line = parser->current.line;
	return (node);
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

static ASTNode	*parse_number(Parser *parser)
{
	ASTNode	*node = arena_alloc(parser->arena, sizeof(ASTNode));
	node->type = AST_NUMBER;
	node->number.value = parser->current.text;
	node->line = parser->current.line;
	return (node);
}

static ASTNode	*parse_grouping(Parser *parser)
{
	ASTNode	*expression = parse_expression(parser, PREC_NONE);
	parser_consume(parser, TOKEN_RPAREN, "Expect ')' after expression.");
	return (expression);
}

static ASTNode	*parse_unary(Parser *parser)
{
	TokenType operator_type = parser->current.type;
	ASTNode *node = arena_alloc(parser->arena, sizeof(ASTNode));

	switch (operator_type)
	{
		case TOKEN_MINUS:	node->type = AST_NEGATE; break;
		case TOKEN_BANG:	node->type = AST_NOT; break;
		default:			node->type = AST_NUMBER; break;
	}

	node->unary.operand = parse_expression(parser, PREC_UNARY);
	return (node);
}

static ASTNode	*parse_binary(Parser *parser, ASTNode *left)
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
		default: break;
	}

	Precedence precedence = get_token_precedence(operator_type);
	node->binary.left = left;
	node->binary.right = parse_expression(parser, (Precedence)(precedence));
	return (node);
}

static ASTNode	*parse_function(Parser *parser)
{
	if (!match(parser, TOKEN_INT))
		return (NULL);

	parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name.");
	StringView func_name = parser->current.text;

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
					.type = TYPE_INT64
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
		.function = {
			.name = func_name,
			.params = params,
			.param_count = param_count,
			.body = body,
			.is_prototype = is_prototype
		}
	};
	return (node);
}

ASTNode	*parser_parse(Lexer *lexer, Arena *arena, ErrorContext *errors)
{
	Parser parser = {0};
	parser.lexer = lexer;
	parser.arena = arena;
	parser.errors = errors;
	parser_advance(&parser);

	ASTNode **declarations = arena_alloc(
			arena, sizeof(ASTNode*) * MAX_FUNCTION_COUNT);
	size_t count = 0;

	while (!check(&parser, TOKEN_EOF))
	{
		if (check(&parser, TOKEN_INT))
			declarations[count++] = parse_function(&parser);
		else
		{
			parser_error(&parser, "Expected function declaration.");
			return (NULL);
		}
	}

	ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_TRANSLATION_UNIT,
		.translation_unit = {
			.declarations = declarations,
			.count = count
		}
	};

	return (node);
}

