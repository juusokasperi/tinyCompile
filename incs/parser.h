#ifndef PARSER_H
# define PARSER_H

# include "error_handler.h"
# include "lexer.h"
# include "ast.h"
# include "memarena.h"
# include "defines.h"
# include <stdbool.h>
# include <stddef.h>

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,	// =
	PREC_OR,			// ||
	PREC_AND,			// &&
	PREC_EQUALITY,		// == =!
	PREC_COMPARISON,	// < > <= >=
	PREC_TERM,			// + -
	PREC_FACTOR,		// * /
	PREC_UNARY,			// ! -
	PREC_CALL,			// . ()
	PREC_PRIMARY,
} Precedence;

typedef struct {
	Lexer			*lexer;
	Arena			*arena;
	ErrorContext	*errors;
	Token			current;
	Token			next;
	bool			panic_mode;
	size_t			expr_depth;
} Parser;

typedef ASTNode*	(*ParseStmtFn)(Parser *p);
typedef ASTNode*	(*ParsePrefixFn)(Parser *p);
typedef ASTNode*	(*ParseInfixFn)(Parser *p, ASTNode *left);

typedef struct {
	const char		*name;
	Precedence		prec;
	ParsePrefixFn	prefix;
	ParseInfixFn	infix;
	ParseStmtFn		stmt;
} ParseRule;

ASTNode	*parser_parse(Lexer *l, Arena *a, ErrorContext *e);

#endif
