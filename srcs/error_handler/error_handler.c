#include "error_handler.h"
#include "layout.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char	*category_names[] = {
	"lexer",
	"parser",
	"semantic",
	"codegen",
	"system"
};

static const char	*level_colors[] = {
	BOLD_GREEN,
	BOLD_RED,
	BOLD_RED
};

static const char	*level_names[] = {
	"warning",
	"error",
	"fatal error"
};

void	error_context_init(ErrorContext *ctx, Arena *arena)
{
	ctx->head = NULL;
	ctx->tail = NULL;
	ctx->error_count = 0;
	ctx->warning_count = 0;
	ctx->arena = arena;
}

void error_add(ErrorContext *ctx, ErrorCategory category, ErrorLevel level,
				const char *filename, int line, int column,
				const char *fmt, ...)
{
	va_list	args;
	va_start(args, fmt);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	size_t len = strlen(buffer);
	char *msg = arena_alloc(ctx->arena, len + 1);
	memcpy(msg, buffer, len + 1);
	ErrorNode *node = arena_alloc(ctx->arena, sizeof(ErrorNode));
	node->category = category;
	node->level = level;
	node->message = msg;
	node->filename = filename;
	node->line = line;
	node->column = column;
	node->next = NULL;

	if (ctx->tail)
	{
		ctx->tail->next = node;
		ctx->tail = node;
	}
	else
		ctx->head = ctx->tail = node;
	
	if (level == ERROR_LEVEL_WARNING)
		ctx->warning_count++;
	else
		ctx->error_count++;
}

bool	error_has_errors(ErrorContext *ctx)
{
	return (ctx->error_count > 0);
}

bool	error_has_fatal(ErrorContext *ctx)
{
	ErrorNode	*curr = ctx->head;
	while (curr)
	{
		if (curr->level == ERROR_LEVEL_FATAL)
			return (true);
		curr = curr->next;
	}
	return (false);
}

void	error_print_all(ErrorContext *ctx)
{
	if (!ctx->head)
		return;
	fprintf(stderr, "\n");
	fprintf(stderr, BOLD_WHITE "Compilation failed with %zu error(s) and %zu warning(s)\n" RESET,
        ctx->error_count, ctx->warning_count);
	fprintf(stderr, "\n");

	ErrorNode	*curr = ctx->head;
	while (curr)
	{
		const char	*color = level_colors[curr->level];
		const char	*level_name = level_names[curr->level];
		if (curr->filename)
		{
			fprintf(stderr, "%s%s:%d:%d: %s[%s]%s %s\n" RESET, BOLD_WHITE,
					curr->filename, curr->line, curr->column, color, level_name,
					RESET, curr->message);
		}
		else
		{
			fprintf(stderr, "%s%s[%s]%s %s\n" RESET, color, category_names[curr->category],
					level_name, RESET, curr->message);
		}
		curr = curr->next;
	}
}
