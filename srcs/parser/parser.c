#include "parser.h"
#include <stdio.h>

static ASTNode	*parse_expression(Parser *p, Precedence precedence);
static ASTNode	*parse_identifier(Parser *p);
static ASTNode	*parse_grouping(Parser *p);
static ASTNode	*parse_number(Parser *p);
static ASTNode	*parse_unary(Parser *p);
static ASTNode	*parse_binary(Parser *p, ASTNode *left);
static ASTNode	*parse_statement(Parser *p);
static ASTNode	*parse_block(Parser *p);
static ASTNode	*parse_function(Parser *p);

static void	p_advance(Parser *p)
{
	p->current = p->next;
	while (true)
	{
		p->next = lexer_next(p->lexer);
		if (p->next.type != TOKEN_ERROR)
			break;
	}
}

static void	consume(Parser *p, TokenType type, const char *message)
{
	if (p->next.type == type)
	{
		p_advance(p);
		return;
	}
	fprintf(stderr, "Error: %s on line %d, col %d\n", message, p->lexer->line, p->lexer->column);
}

static bool inline match(Parser *p, TokenType type)
{
	if (p->next.type == type)
	{
		p_advance(p);
		return (true);
	}
	return (false);
}

static bool inline check(Parser *p, TokenType type)
{
	return (p->next.type == type);
}

static Precedence	get_token_precedence(TokenType type)
{
	switch (type)
	{
		case TOKEN_EQUAL:	return PREC_ASSIGNMENT;
		case TOKEN_PLUS:
		case TOKEN_MINUS:	return PREC_TERM;
		case TOKEN_STAR:
		case TOKEN_SLASH:	return PREC_FACTOR;
		default:			return PREC_NONE;
	}
}

static ASTNode	*parse_identifier(Parser *p)
{
	ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
	node->type = AST_IDENTIFIER;
	node->identifier.name = p->current.text;
	node->line = p->current.line;
	return (node);
}

static ASTNode	*parse_statement(Parser *p)
{
	if (match(p, TOKEN_INT))
	{
        consume(p, TOKEN_IDENTIFIER, "Expected variable name");
        StringView var_name = p->current.text;

		ASTNode *init = NULL;
		if (match(p, TOKEN_EQUAL))
			init = parse_expression(p, PREC_NONE);

        consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");

        ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
        *node = (ASTNode){
            .type = AST_VAR_DECL,
            .var_decl = {
                .var_name = var_name,
                .initializer = init
            }
        };
        return (node);
	}
	if (match(p, TOKEN_RETURN))
	{
        ASTNode *expr = parse_expression(p, PREC_NONE);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after return");
        
        ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
        *node = (ASTNode){
            .type = AST_RETURN,
            .return_stmt = { .expression = expr }
        };
        return (node);
    }
	ASTNode *expr = parse_expression(p, PREC_NONE);
    consume(p, TOKEN_SEMICOLON, "Expected ';' after expression");
    return (expr);
}

ASTNode* parse_block(Parser *p)
{
    consume(p, TOKEN_LBRACE, "Expected '{'");
    
    ASTNode **stmts = arena_alloc(p->arena, sizeof(ASTNode*) * 256);
    size_t count = 0;
    
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        stmts[count++] = parse_statement(p);
    }
    
    consume(p, TOKEN_RBRACE, "Expected '}'");
    ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
    *node = (ASTNode){
        .type = AST_BLOCK,
        .block = {
            .statements = stmts,
            .count = count
        }
    };
    return (node);
}

static ASTNode	*parse_prefix(Parser *p)
{
	switch (p->current.type)
	{
		case TOKEN_IDENTIFIER:	return (parse_identifier(p));
		case TOKEN_NUMBER:		return (parse_number(p));
		case TOKEN_LPAREN:		return (parse_grouping(p));
		case TOKEN_MINUS:		return (parse_unary(p));
		default:
			fprintf(stderr, "Expect expression at line %d (Token %.*s).\n",
					p->current.line, (int)p->current.text.len, p->current.text.start);
			return (NULL);
	}
}

static ASTNode	*parse_infix(Parser *p, ASTNode	*left)
{
	TokenType operator_type = p->current.type;
	if (operator_type == TOKEN_EQUAL)
	{
		ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
		node->type = AST_ASSIGNMENT;
		node->assignment.var_name = left->identifier.name;
		node->assignment.value = parse_expression(p, PREC_ASSIGNMENT);
		return (node);
	}

	switch (operator_type)
	{
		case TOKEN_PLUS:
		case TOKEN_MINUS:
		case TOKEN_STAR:
		case TOKEN_SLASH:
			return (parse_binary(p, left));
		default:
			return (left);
	}
}

static ASTNode	*parse_expression(Parser *p, Precedence prec)
{
	p_advance(p);

	ASTNode *left = parse_prefix(p);
	if (!left)
		return (NULL);
	while (prec < get_token_precedence(p->next.type))
	{
		p_advance(p);
		left = parse_infix(p, left);
	}

	return (left);
}

static ASTNode	*parse_number(Parser *p)
{
	ASTNode	*node = arena_alloc(p->arena, sizeof(ASTNode));
	node->type = AST_NUMBER;
	node->number.value = p->current.text;
	node->line = p->current.line;
	return (node);
}

static ASTNode	*parse_grouping(Parser *p)
{
	ASTNode	*expression = parse_expression(p, PREC_NONE);
	consume(p, TOKEN_RPAREN, "Expect ')' after expression.");
	return (expression);
}

static ASTNode	*parse_unary(Parser *p)
{
	TokenType operator_type = p->current.type;
	ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));

	switch (operator_type)
	{
		case TOKEN_MINUS:	node->type = AST_NEGATE; break;
		case TOKEN_BANG:	node->type = AST_NOT; break;
		default:			node->type = AST_NUMBER; break;
	}

	node->unary.operand = parse_expression(p, PREC_UNARY);
	return (node);
}

static ASTNode	*parse_binary(Parser *p, ASTNode *left)
{
	TokenType operator_type = p->current.type;
	ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));

	switch (operator_type)
	{
		case TOKEN_PLUS:	node->type = AST_ADD; break;
		case TOKEN_MINUS:	node->type = AST_SUB; break;
		case TOKEN_STAR:	node->type = AST_MUL; break;
		case TOKEN_SLASH:	node->type = AST_DIV; break;
		default: break;
	}

	Precedence prec = get_token_precedence(operator_type);
	node->binary.left = left;
	node->binary.right = parse_expression(p, (Precedence)(prec));
	return (node);
}

static ASTNode	*parse_function(Parser *p)
{
	if (!match(p, TOKEN_INT))
		return (NULL);

	consume(p, TOKEN_IDENTIFIER, "Expected function name.");
	StringView func_name = p->current.text;

	consume(p, TOKEN_LPAREN, "Expected '(' after function name.");
	if (check(p, TOKEN_IDENTIFIER) && sv_eq_cstr(p->next.text, "void"))
		p_advance(p);
	
	consume(p, TOKEN_RPAREN, "Expected ')' after parameters.");

	ASTNode *body = parse_block(p);
	ASTNode *node = arena_alloc(p->arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_FUNCTION,
		.function = {
			.name = func_name,
			.params = NULL,	// params not supported yet
			.param_count = 0,
			.body = body
		}
	};
	return (node);
}

ASTNode	*parse(Lexer *l, Arena *a)
{
	Parser p = {0};
	p.lexer = l;
	p.arena = a;
	p_advance(&p);

	if (check(&p, TOKEN_INT))
		return (parse_function(&p));
	if (check(&p, TOKEN_LBRACE))
		return (parse_block(&p));
	else
		return (parse_expression(&p, PREC_NONE));
}
