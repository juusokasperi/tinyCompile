#include "parser_internal.h"
#include "ast.h"
#include "defines.h"
#include "error_handler.h"
#include "lexer.h"

const ParseRule rules[] = {
	#define X_TOKEN(name, str, is_keyword, prec, prefix, infix, stmt) {str, prec, prefix, infix, stmt},
	#include "lexer_tokens.def"
	#undef X_TOKEN
};

ASTNode	*parser_parse(Lexer *lexer, Arena *arena, ErrorContext *errors)
{
	Parser parser = {0};
	parser.lexer = lexer;
	parser.arena = arena;
	parser.errors = errors;
	parser_advance(&parser);

	ASTNode **declarations = arena_alloc(
			arena, sizeof(ASTNode*) * MAX_FUNCTION_COUNT);
	size_t count = 0;

	while (!check(&parser, TOKEN_EOF))
	{
		if (check(&parser, TOKEN_INT))
			declarations[count++] = parse_function(&parser);
		else
		{
			parser_error(&parser, "Expected function declaration.");
			return (NULL);
		}
	}

	ASTNode *node = arena_alloc(arena, sizeof(ASTNode));
	*node = (ASTNode){
		.type = AST_TRANSLATION_UNIT,
		.translation_unit = {
			.declarations = declarations,
			.count = count
		}
	};

	return (node);
}

const char	*token_type_name(TokenType type)
{
	size_t rule_count = sizeof(rules) / sizeof(rules[0]);
	if (type < 0 || (size_t)type >= rule_count)
		return ("UNKNOWN");
	return (rules[type].name);
}

Precedence	get_token_precedence(TokenType type)
{
	size_t rule_count = sizeof(rules) / sizeof(rules[0]);
	if (type < 0 || (size_t)type >= rule_count)
		return (PREC_NONE);
	return (rules[type].prec);
}
