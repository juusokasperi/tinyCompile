#ifndef COMPILE_H
# define COMPILE_H

# include "ast.h"
# include "file_map.h"
# include "memarena.h"
# include "semantic.h"
# include "error_handler.h"
# include "cleanup.h"
# include <stdbool.h>
# include <stddef.h>

typedef struct CompilationUnit {
	FileMap	file;
	ASTNode	*ast;
	bool	parsed_ok;
} CompilationUnit;

typedef struct {
	CompilationUnit	*units;
	size_t			count;
	size_t			capacity;
	Arena			*arena;
	ErrorContext	*errors;
	GlobalScope		global;
} CompilationContext;

bool	compile_ctx_init(CompilationContext *ctx, Arena *arena, 
					ErrorContext *errors, size_t file_count);
bool	compile_ctx_add_file(CompilationContext *ctx, const char *filepath,
					ResourceTracker *resources);
bool	compile_parse_all(CompilationContext *ctx);
bool	compile_analyze_all(CompilationContext *ctx);
void	compile_print_errors(CompilationContext *ctx);
ASTNode	*compile_get_entry_point(CompilationContext *ctx);

#endif
