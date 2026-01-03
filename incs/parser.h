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
						//
	PREC_BIT_OR,		// |
	PREC_BIT_XOR,		// ^
	PREC_BIT_AND,		// &
						//
	PREC_EQUALITY,		// == =!
	PREC_COMPARISON,	// < > <= >=
	PREC_SHIFT,			// << >>
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

ASTNode	*parser_parse(Lexer *l, Arena *a, ErrorContext *e);

#endif
