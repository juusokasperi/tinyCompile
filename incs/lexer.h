#ifndef LEXER_H
# define LEXER_H

# include <ctype.h>
# include <stdbool.h>
# include "file_map.h"
# include "string_view.h"

typedef enum {
	TOKEN_EOF,

	TOKEN_INT,

	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_WHILE,
	TOKEN_RETURN,

	TOKEN_NUMBER,
	TOKEN_IDENTIFIER,

	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_STAR,
	TOKEN_SLASH,

	TOKEN_BANG,

	TOKEN_EQUAL_EQUAL,
	TOKEN_BANG_EQUAL,
	TOKEN_LESS,
	TOKEN_LESS_EQUAL,
	TOKEN_GREATER,
	TOKEN_GREATER_EQUAL,

	TOKEN_EQUAL,
	TOKEN_SEMICOLON,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACE,
	TOKEN_RBRACE,

	TOKEN_COMMA,

	TOKEN_ERROR,
} TokenType;

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
	int 		line;
	int 		column;
	FileMap		*file;
} Lexer;

/* =================== */
/* FUNCTION PROTOTYPES */
/* =================== */

void	lexer_init(Lexer *l, FileMap *file);
Token	lexer_next(Lexer *l);
char	peek(Lexer *l);
char	peek_next(Lexer *l);
char	advance(Lexer *l);
void	skip_whitespace(Lexer *l);
Token	make_token(Lexer *l, TokenType type, StringView text);
Token	make_token_no_sv(Lexer *l, TokenType type);

#endif
