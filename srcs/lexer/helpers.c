/**
 * @file
 * @brief Helper functions for lexer
 */

#include "lexer.h"

static inline bool is_comment(Lexer *l);

void lexer_init(Lexer *l, FileMap *file)
{
	l->file = file;
	l->start = file->data;
	l->curr = file->data;
	l->end = file->data + file->length;
	l->line = 1;
	l->column = 1;
}

/**
 * @brief Looks at the next char in the file we are tokenizing without advancing
 *
 * @param l 
 * @return next character	
 */
char lexer_peek(Lexer *l)
{
	if (l->curr >= l->end)
		return ('\0');
	return (*l->curr);
}

/**
 * @brief Looks at the char + 2 in the file we are tokenizing without advancing
 *
 * @param l 
 */
char lexer_peek_next(Lexer *l)
{
	if (l->curr + 1 >= l->end)
		return ('\0');
	return (*(l->curr + 1));
}

char lexer_advance(Lexer *l)
{
	l->curr++;
	l->column++;
	return l->curr[-1];
}

/**
 * @brief Skips whitespaces and comments
 *
 * @param l 
 */
void lexer_skip_whitespace(Lexer *l)
{
	while (l->curr < l->end)
	{
		char c = *l->curr;
		switch (c)
		{
			case ' ':
			case '\r':
			case '\t':
				lexer_advance(l);
				break;
			case '\n':
				l->line++;
				l->column = 1;
				lexer_advance(l);
				break;
			case '/':
				if (is_comment(l))
					break;
				else				// is division
					return;
			case '#':
				while (lexer_peek(l) != '\n' && l->curr < l->end)
					lexer_advance(l);
				break;
			default:
				return;
		}
	}
}

static inline bool is_comment(Lexer *l)
{
	bool result = false;
	if (lexer_peek_next(l) == '/')
	{
		result = true;
		while (lexer_peek(l) != '\n' && l->curr < l->end)
			lexer_advance(l);
	}
	else if (lexer_peek_next(l) == '*')
	{
		result = true;
		lexer_advance(l);
		lexer_advance(l);
		while (l->curr < l->end)
		{
			if (lexer_peek(l) == '*' && lexer_peek_next(l) == '/')
			{
				lexer_advance(l);
				lexer_advance(l);
				break;
			}
			if (lexer_peek(l) == '\n')
			{
				l->line++;
				l->column = 1;
			}
			lexer_advance(l);
		}
	}
	return (result);
}

/**
 * @brief Makes a token containing a text field
 *
 * @param l 
 * @param type 
 * @param text 
 * @return new token for parser
 */
Token	lexer_make_token(Lexer *l, TokenType type, StringView text)
{
	Token result = {
		.type = type,
		.text = text,
		.line = l->line,
		.column = l->column 
	};
	return (result);
}

/**
 * @brief Makes a token without a text field
 *
 * @param l 
 * @param type 
 * @return new token for parser
 */
Token	lexer_make_token_no_sv(Lexer *l, TokenType type)
{
	Token result = {
		.type = type,
		.line = l->line,
		.column = l->column 
	};
	return (result);
}
