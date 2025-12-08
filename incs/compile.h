#ifndef COMPILE_H
# define COMPILE_H

# include "ast.h"
# include "lexer.h"
# include "parser.h"
# include "memarena.h"
# include "shared_types.h"
# include "semantic.h"
# include <stdbool.h>

typedef struct {
	FileMap	file;
	ASTNode	*ast;
	bool	parsed_ok;
} CompilationUnit;

typedef struct {
	CompilationUnit	*units;
	size_t 			count;
	size_t			capacity;
	Arena			*arena;
	ErrorList		errors;
	GlobalScope		global;
} CompilationContext;

void	compile_ctx_init(CompilationContext *ctx, Arena *arena, size_t file_count);
bool	compile_ctx_add_file(CompilationContext *ctx, const char *filepath);
bool	compile_parse_all(CompilationContext *ctx);
bool	compile_analyze_all(CompilationContext *ctx);
void	compile_print_errors(CompilationContext *ctx);
ASTNode	*compile_get_entry_point(CompilationContext *ctx);

#endif
