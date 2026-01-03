#include "parser_internal.h"

void	parser_advance(Parser *parser)
{
	parser->current = parser->next;
	while (true)
	{
		parser->next = lexer_next(parser->lexer);
		if (parser->next.type != TOKEN_ERROR)
			break;
	}
}

void	parser_consume(Parser *parser, TokenType type, const char *message)
{
	if (parser->next.type == type)
	{
		parser_advance(parser);
		parser->panic_mode = false;
		return;
	}
	parser_error(parser, "%s (got %s)", message, 
			token_type_name(parser->next.type));
	parser_synchronize(parser);
}

void parser_error(Parser *parser, const char *fmt, ...)
{
	if (parser->panic_mode)
		return;
	parser->panic_mode = true;
	va_list	args;
	va_start(args, fmt);
	char	buffer[512];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	error_parser(parser->errors, parser->lexer->file->name,
			parser->current.line, parser->current.column,
			"%s", buffer);
}

void	parser_synchronize(Parser *parser)
{
	parser->panic_mode = false;
	while (parser->next.type != TOKEN_EOF)
	{
		if (parser->current.type == TOKEN_SEMICOLON)
			return;
		switch (parser->next.type)
		{
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_RETURN:
			case TOKEN_INT:
			case TOKEN_LBRACE:
				return;
			default:
				break;
		}
		parser_advance(parser);
	}
}
bool match(Parser *parser, TokenType type)
{
	if (parser->next.type == type)
	{
		parser_advance(parser);
		return (true);
	}
	return (false);
}

bool check(Parser *parser, TokenType type)
{
	return (parser->next.type == type);
}

DataType	parse_type(Parser *parser)
{
	if (is_type_keyword(parser->next.text))
	{
		parser_advance(parser);
		return (type_from_sv(parser->current.text));
	}

	parser_error(parser, "expected type specifier (e.g. 'int', 'char', 'void')");
	return (TYPE_VOID);
}

