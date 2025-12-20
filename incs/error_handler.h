#ifndef ERROR_HANDLER_H
# define ERROR_HANDLER_H

# include "memarena.h"
# include <stdbool.h>
# include <stdarg.h>
#include <stddef.h>

typedef enum {
	ERROR_LEXER,
	ERROR_PARSER,
	ERROR_SEMANTIC,
	ERROR_CODEGEN,
	ERROR_SYSTEM,
} ErrorCategory;

typedef enum {
	ERROR_LEVEL_WARNING,
	ERROR_LEVEL_ERROR,
	ERROR_LEVEL_FATAL,
} ErrorLevel;

typedef struct ErrorNode ErrorNode;

struct ErrorNode {
	ErrorCategory	category;
	ErrorLevel		level;
	const char		*message;
	const char		*filename;
	int				line;
	int				column;
	ErrorNode		*next;
};

typedef struct {
	ErrorNode	*head;
	ErrorNode	*tail;
	size_t		error_count;
	size_t		warning_count;
	Arena		*arena;
} ErrorContext;

void	error_context_init(ErrorContext *ctx, Arena *a);
void	error_add(ErrorContext *ctx, ErrorCategory category, ErrorLevel level,
			const char *filename, int line, int column, 
			const char *fmt, ...) __attribute__((format(printf, 7, 8)));

#define error_lexer(ctx, file, line, col, ...) \
	error_add(ctx, ERROR_LEXER, ERROR_LEVEL_ERROR, file, line, col, __VA_ARGS__)
#define error_parser(ctx, file, line, col, ...) \
	error_add(ctx, ERROR_PARSER, ERROR_LEVEL_ERROR, file, line, col, __VA_ARGS__)
#define error_semantic(ctx, file, line, col, ...) \
	error_add(ctx, ERROR_SEMANTIC, ERROR_LEVEL_ERROR, file, line, col, __VA_ARGS__)
#define error_fatal(ctx, file, line, col, ...) \
	error_add(ctx, ERROR_SYSTEM, ERROR_LEVEL_FATAL, file, line, col, __VA_ARGS__)
#define warning_add(ctx, file, line, col, ...) \
	error_add(ctx, ERROR_PARSER, ERROR_LEVEL_WARNING, file, line, col, __VA_ARGS__)

bool	error_has_errors(ErrorContext *ctx);
bool	error_has_fatal(ErrorContext *ctx);
void	error_print_all(ErrorContext *ctx);

#endif // ERROR_HANDLER_H
