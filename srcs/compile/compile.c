#include "compile.h"
#include "utils.h"
#include <stdio.h>

void compile_ctx_init(CompilationContext *ctx, Arena *arena, size_t file_count)
{
	ctx->arena = arena;
	ctx->capacity = file_count > 0 ? file_count : 8;
	ctx->count = 0;
	ctx->units = arena_alloc(arena, sizeof(CompilationUnit) * ctx->capacity);
	error_list_init(&ctx->errors);
	global_scope_init(&ctx-> global);
}

bool compile_ctx_add_file(CompilationContext *ctx, const char *filepath)
{
	if (ctx->count >= ctx->capacity)
	{
		fprintf(stderr, "Too many files (max %zu)\n", ctx->capacity);
		return (false);
	}
	int fd = safe_open((char *)filepath);
	FileMap file = map_input(fd);
	if (!file.data)
	{
		fprintf(stderr, "Failed to map file: %s\n", filepath);
		return (false);
	}
	file.name = filepath;
	ctx->units[ctx->count] = (CompilationUnit){
		.file = file,
		.ast = NULL,
		.parsed_ok = false
	};
	ctx->count++;

	printf("Added file: %s (%zu bytes)\n", filepath, file.length);
	return (true);
}

bool compile_parse_all(CompilationContext *ctx)
{
	bool all_ok = true;
	printf("\n=== Parsing Phase ===\n");

	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		printf ("Parsing %s...\n", unit->file.name);
		Lexer lexer;
		lexer_init(&lexer, &unit->file);
		unit->ast = parse(&lexer, ctx->arena);
		unit->parsed_ok = (unit->ast != NULL);
		if (!unit->parsed_ok)
		{
			fprintf(stderr, "Parse failed for %s\n", unit->file.name);
			all_ok = false;
		}
	}

	return (all_ok);
}

bool compile_analyze_all(CompilationContext *ctx)
{
	bool all_ok = true;

	printf("\n=== Semantic Analysis Phase ===\n");
    
	printf("Pass 1: Collecting function declarations..\n");
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->parsed_ok)
			continue;
		if (unit->ast && unit->ast->type == AST_FUNCTION)
		{
			if (!global_declare_function(&ctx->global, ctx->arena, &ctx->errors,
						unit->ast->function.name, TYPE_INT32, unit->ast->function.params,
						unit->ast->function.param_count, unit->ast->line, unit->file.name))
				all_ok = false;
		}
		if (!all_ok)
			return (false);
	}
	
	printf("Pass 1: Analyzing function bodies..\n");
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->parsed_ok)
			continue;
		printf("Analyzing %s...\n", unit->file.name);
		if (!semantic_analyze(ctx->arena, unit->ast, &ctx->errors, 
					unit->file.name, &ctx->global))
			all_ok = false;
	}

	return (all_ok);
}

void compile_print_errors(CompilationContext *ctx)
{
	if (ctx->errors.count > 0)
	{
		fprintf(stderr, "\n=== Compilation Errors (%zu) ===\n", ctx->errors.count);
		error_list_print(&ctx->errors);
	}
}

ASTNode *compile_get_entry_point(CompilationContext *ctx)
{
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->ast || !unit->parsed_ok)
			continue;
		if (unit->ast->type == AST_FUNCTION)
			if (sv_eq_cstr(unit->ast->function.name, "main"))
				return (unit->ast);
	}

	return (NULL);
}
