#include "lexer.h"

static Token	make_token(Lexer *l, TokenType type, StringView text)
{
	Token result = {
		.type = type,
		.text = text,
		.line = l->line,
		.column = l->column 
	};
	return (result);
}

Token lexer_next(Lexer *l)
{
	skip_whitespace(l);

	if (l->curr >= l->end)
		return (Token){.type = TOKEN_EOF, .line = l->line, .column = l->column};

	char c = *l->curr;
	if (isdigit(c))
	{
		const char *start = l->curr;
		while (l->curr < l->end && isdigit(*l->curr))
			advance(l);
		StringView text = { .start = start, .len = l->curr - start };
		return (make_token(l, TOKEN_NUMBER, text));
	}

	if (isalpha(c) || c == '_')
	{
		const char *start = l->curr;
		while (l->curr < l->end && (isalnum(*l->curr) || *l->curr == '_'))
			advance(l);

		StringView text = {.start = start, .len = l->curr - start};
		if (sv_eq_cstr(text, "int"))
			return (make_token(l, TOKEN_INT, text));
		if (sv_eq_cstr(text, "return"))
			return (make_token(l, TOKEN_RETURN, text));
		if (sv_eq_cstr(text, "if"))
            return (make_token(l, TOKEN_IF, text));
        if (sv_eq_cstr(text, "else"))
            return (make_token(l, TOKEN_ELSE, text));
        if (sv_eq_cstr(text, "while"))
            return (make_token(l, TOKEN_WHILE, text));

		return (make_token(l, TOKEN_IDENTIFIER, text));
	}

	advance(l);
	StringView text = { .start = l->curr - 1, .len = 1 };

	switch (c)
	{
		case '+': return (make_token(l, TOKEN_PLUS, text));
        case '-': return (make_token(l, TOKEN_MINUS, text));
        case '*': return (make_token(l, TOKEN_STAR, text));
        case '/': return (make_token(l, TOKEN_SLASH, text));
        case '(': return (make_token(l, TOKEN_LPAREN, text));
        case ')': return (make_token(l, TOKEN_RPAREN, text));
        case '{': return (make_token(l, TOKEN_LBRACE, text));
        case '}': return (make_token(l, TOKEN_RBRACE, text));
        case ';': return (make_token(l, TOKEN_SEMICOLON, text));
		case '!':
			if (peek(l) == '=')
			{
				advance(l);
				return (Token){.type = TOKEN_EQUAL_EQUAL, .line = l->line};
			}
			return (Token){.type = TOKEN_EQUAL, .line = l->line};
		case '>':
			if (peek(l) == '=')
			{
				advance(l);
				return (Token){.type = TOKEN_GREATER_EQUAL, .line = l->line};
			}
			return (Token){.type = TOKEN_GREATER, .line = l->line};
		case '<':
			if (peek(l) == '=')
			{
				advance(l); 
				return (Token){.type = TOKEN_LESS_EQUAL, .line = l->line};
			}
			return (Token){.type = TOKEN_LESS, .line = l->line};
		default:  return (Token){.type = TOKEN_ERROR, .line = l->line};
	}
}
