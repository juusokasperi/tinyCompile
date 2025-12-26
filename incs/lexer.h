#ifndef LEXER_H
# define LEXER_H

# include <ctype.h>
# include <stdbool.h>
# include "file_map.h"
# include "string_view.h"

typedef enum {
	#define X_TOKEN(name, str, is_keyword, prec, prefix_fn, infix_fn, stmt_fn) name,
	#include "lexer_tokens.def"
	#undef X_TOKEN
}	TokenType;

typedef struct {
	TokenType type;
	StringView text;
	int line;
	int column;
} Token;

typedef struct {
	const char	*start;
	const char	*curr;
	const char	*end;
	int			line;
	int			column;
	FileMap		*file;
} Lexer;

/* =================== */
/* FUNCTION PROTOTYPES */
/* =================== */

void	lexer_init(Lexer *l, FileMap *file);
Token	lexer_next(Lexer *l);
char	lexer_peek(Lexer *l);
char	lexer_peek_next(Lexer *l);
char	lexer_advance(Lexer *l);
void	lexer_skip_whitespace(Lexer *l);
Token	lexer_make_token(Lexer *l, TokenType type, StringView text);
Token	lexer_make_token_no_sv(Lexer *l, TokenType type);

#endif
