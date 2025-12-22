#include "lexer.h"

static TokenType check_keyword(StringView text);

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

		StringView	text = {.start = start, .len = l->curr - start};
		TokenType	type = check_keyword(text);

		return (lexer_make_token(l, type, text));
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

static TokenType check_keyword(StringView text)
{
	#define X_TOKEN(name, str, is_keyword, prec, prefix_fn, infix_fn, stmt_fn) \
		if (is_keyword && sv_eq_cstr(text, str)) return (name);
	#include "lexer_tokens.def"
	#undef X_TOKEN
	return (TOKEN_IDENTIFIER);
}
