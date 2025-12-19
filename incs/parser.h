#ifndef PARSER_H
# define PARSER_H

# include "lexer.h"
# include "ast.h"
# include "memarena.h"
# include "defines.h"	// for MAX_ARGS
# include <stdbool.h>

typedef struct {
	Lexer	*lexer;
	Arena	*arena;
	Token	current;
	Token	next;
	bool	had_error;
	bool	panic_mode;
} Parser;

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

ASTNode	*parse(Lexer *l, Arena *a);

#endif
