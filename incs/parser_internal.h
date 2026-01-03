#ifndef PARSER_INTERNAL_H
# define PARSER_INTERNAL_H

# include "parser.h"
# include "ast.h"
# include "lexer.h"

typedef ASTNode*	(*ParsePrefixFn)(Parser *p);
typedef ASTNode*	(*ParseInfixFn)(Parser *p, ASTNode *left);
typedef ASTNode*	(*ParseStmtFn)(Parser *p);

typedef struct {
	const char		*name;
	Precedence		prec;
	ParsePrefixFn	prefix;
	ParseInfixFn	infix;
	ParseStmtFn	stmt;
} ParseRule;

extern const ParseRule rules[];

// === Expression parsing (parser_expr.c) ===
ASTNode		*parse_expression(Parser *parser, Precedence precedence);
ASTNode		*parse_assignment(Parser *p, ASTNode *left);
ASTNode		*parse_identifier(Parser *parser);
ASTNode		*parse_grouping(Parser *parser);
ASTNode		*parse_number(Parser *parser);
ASTNode		*parse_unary(Parser *parser);
ASTNode		*parse_binary(Parser *parser, ASTNode *left);
ASTNode		*parse_call(Parser *parser, ASTNode *callee);

// === Statement parsing (parser_stmt.c) ===
ASTNode		*parse_statement(Parser *parser);
ASTNode		*parse_block(Parser *parser);
ASTNode		*parse_function(Parser *parser);
ASTNode		*parse_var_decl(Parser *parser);
ASTNode		*parse_return(Parser *parser);
ASTNode		*parse_if(Parser *parser);
ASTNode		*parse_while(Parser *parser);
ASTNode		*parse_expr_stmt(Parser *parser);

// === Utils (parser_utils.c ===
void		parser_advance(Parser *parser);
void		parser_consume(Parser *parser, TokenType type, const char *message);
void		parser_error(Parser *parser, const char *fmt, ...);
void		parser_synchronize(Parser *parser);
bool		match(Parser *parser, TokenType type);
bool		check(Parser *parser, TokenType type);
const char	*token_type_name(TokenType type);
Precedence	get_token_precedence(TokenType type);
DataType	parse_type(Parser *parser);




#endif // PARSER_INTERNAL_H
