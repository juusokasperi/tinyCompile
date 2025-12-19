#include "lexer.h"

/**
 * @brief Lexer works on a as-requested basis,
 * returns the next token for the parser
 *
 * @param l 
 * @return Token
 */
Token lexer_next(Lexer *l)
{
	lexer_skip_whitespace(l);

	if (l->curr >= l->end)
		return (lexer_make_token_no_sv(l, TOKEN_EOF));

	char c = *l->curr;
	if (isdigit(c))
	{
		const char *start = l->curr;
		while (l->curr < l->end && isdigit(*l->curr))
			lexer_advance(l);
		StringView text = { .start = start, .len = l->curr - start };
		return (lexer_make_token(l, TOKEN_NUMBER, text));
	}

	if (isalpha(c) || c == '_')
	{
		const char *start = l->curr;
		while (l->curr < l->end && (isalnum(*l->curr) || *l->curr == '_'))
			lexer_advance(l);

		StringView text = {.start = start, .len = l->curr - start};
		if (sv_eq_cstr(text, "int"))
			return (lexer_make_token(l, TOKEN_INT, text));
		if (sv_eq_cstr(text, "return"))
			return (lexer_make_token(l, TOKEN_RETURN, text));
		if (sv_eq_cstr(text, "if"))
            return (lexer_make_token(l, TOKEN_IF, text));
        if (sv_eq_cstr(text, "else"))
            return (lexer_make_token(l, TOKEN_ELSE, text));
        if (sv_eq_cstr(text, "while"))
            return (lexer_make_token(l, TOKEN_WHILE, text));

		return (lexer_make_token(l, TOKEN_IDENTIFIER, text));
	}

	lexer_advance(l);
	StringView text = { .start = l->curr - 1, .len = 1 };

	switch (c)
	{
		case '+': return (lexer_make_token(l, TOKEN_PLUS, text));
        case '-': return (lexer_make_token(l, TOKEN_MINUS, text));
        case '*': return (lexer_make_token(l, TOKEN_STAR, text));
        case '/': return (lexer_make_token(l, TOKEN_SLASH, text));
        case '(': return (lexer_make_token(l, TOKEN_LPAREN, text));
        case ')': return (lexer_make_token(l, TOKEN_RPAREN, text));
        case '{': return (lexer_make_token(l, TOKEN_LBRACE, text));
        case '}': return (lexer_make_token(l, TOKEN_RBRACE, text));
        case ';': return (lexer_make_token(l, TOKEN_SEMICOLON, text));
		case ',': return (lexer_make_token(l, TOKEN_COMMA, text));
		case '!':
			if (lexer_peek(l) == '=')
			{
				lexer_advance(l);
				return (lexer_make_token_no_sv(l, TOKEN_BANG_EQUAL));
			}
			return (lexer_make_token_no_sv(l, TOKEN_BANG));
		case '=':
			if (lexer_peek(l) == '=')
			{
				lexer_advance(l);
				return (lexer_make_token_no_sv(l, TOKEN_EQUAL_EQUAL));
			}
			return (lexer_make_token_no_sv(l, TOKEN_EQUAL));
		case '>':
			if (lexer_peek(l) == '=')
			{
				lexer_advance(l);
				return (lexer_make_token_no_sv(l, TOKEN_GREATER_EQUAL));
			}
			return (lexer_make_token_no_sv(l, TOKEN_GREATER));
		case '<':
			if (lexer_peek(l) == '=')
			{
				lexer_advance(l); 
				return (lexer_make_token_no_sv(l, TOKEN_LESS_EQUAL));
			}
			return (lexer_make_token_no_sv(l, TOKEN_LESS));
		default:  return (lexer_make_token_no_sv(l, TOKEN_ERROR));
	}
}
