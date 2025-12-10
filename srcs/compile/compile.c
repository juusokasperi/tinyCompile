
#include "compile.h"
#include "layout.h"
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
		fprintf(stderr, BOLD_RED "  > too many files (max %zu)\n" RESET, ctx->capacity);
		return (false);
	}
	int fd = safe_open((char *)filepath);
	FileMap file = map_input(fd);
	if (!file.data)
	{
		fprintf(stderr, BOLD_RED "  > failed to map file: %s\n" RESET, filepath);
		return (false);
	}
	file.name = filepath;
	ctx->units[ctx->count] = (CompilationUnit){
		.file = file,
		.ast = NULL,
		.parsed_ok = false
	};
	ctx->count++;

	printf(BOLD_GREEN "> LOAD SOURCE: " RESET WHITE "%s (%zu bytes)\n" RESET, filepath, file.length);
	return (true);
}

bool compile_parse_all(CompilationContext *ctx)
{
	bool all_ok = true;

	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		printf ("  > parsing %s\n", unit->file.name);
		Lexer lexer;
		lexer_init(&lexer, &unit->file);
		unit->ast = parse(&lexer, ctx->arena);
		unit->parsed_ok = (unit->ast != NULL);
		if (!unit->parsed_ok)
		{
			fprintf(stderr, BOLD_RED "  > parse failed for %s\n" RESET, unit->file.name);
			all_ok = false;
		}
	}

	return (all_ok);
}

bool compile_analyze_all(CompilationContext *ctx)
{
	bool all_ok = true;

	printf("  > collecting function declarations\n");
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->parsed_ok)
			continue;
		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			if (func->type == AST_FUNCTION)
			{
				if (!global_declare_function(&ctx->global, ctx->arena, &ctx->errors, func, unit->file.name))
					all_ok = false;
			}
		}
		if (!all_ok)
			return (false);
	}
	
	printf("  > analyzing function bodies\n");
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->parsed_ok)
			continue;
		printf("%4zu | %s\n", i, unit->file.name);
		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			if (!semantic_analyze(ctx->arena, func, &ctx->errors, 
						unit->file.name, &ctx->global))
				all_ok = false;
		}
	}

	return (all_ok);
}

void compile_print_errors(CompilationContext *ctx)
{
	if (ctx->errors.count > 0)
	{
		char *msg = arena_sprintf(ctx->arena, "COMPILE ERRORS (%zu)", ctx->errors.count);	
		print_phase(-1, msg);
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
		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			if (func->type == AST_FUNCTION && sv_eq_cstr(func->function.name, "main"))
				return (func);
		}
	}

	return (NULL);
}
