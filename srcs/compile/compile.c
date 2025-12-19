#include "compile.h"
#include "cleanup.h"
#include "layout.h"
#include "utils.h"
#include <stdio.h>
#include "bounds.h"
#include "lexer.h"
#include "validation.h"
#include "parser.h"
#include <errno.h>

bool compile_ctx_init(CompilationContext *ctx, Arena *arena, 
		ErrorContext *errors, size_t file_count)
{
	ctx->arena = arena;
	ctx->errors = errors;
	ctx->count = 0;
	if (file_count > MAX_SOURCE_FILES)
	{
		error_fatal(errors, NULL, 0, 0,
				"Too many files (max %d)", MAX_SOURCE_FILES);
		return (false);
	}
	ctx->capacity = file_count > 0 ? file_count : 8;
	ctx->units = arena_alloc(arena, sizeof(CompilationUnit) * ctx->capacity);
	if (!ctx->units)
	{
		error_fatal(errors, NULL, 0, 0,
				"Failed to allocate compilation units");
		return (false);
	}
	semantic_global_init(&ctx->global);
	return (true);
}

bool compile_ctx_add_file(CompilationContext *ctx, const char *filepath,
		ResourceTracker *resources, int *out_fd, void **out_mapped, size_t *out_size)
{
	if (ctx->count >= ctx->capacity)
	{
		error_fatal(ctx->errors, NULL, 0, 0, "Internal error: compilation unit overflow");
		return (false);
	}
	FileValidation validation;
	if (!validate_source_file(filepath, &validation, ctx->errors))
		return (false);

	int fd = safe_open(validation.canonical_path);
	if (fd == -1)
	{
		error_fatal(ctx->errors, filepath, 0, 0,
				"Failed to open file: %s", strerror(errno));
		return (false);
	}

	FileMap file = map_input(fd);
	if (!file.data)
	{
		error_fatal(ctx->errors, filepath, 0, 0, "Failed to map file into memory");
		return (false);
	}
	file.name = filepath;
	resource_track_mmap(resources, (void *)file.data, file.length);
	ctx->units[ctx->count] = (CompilationUnit){
		.file = file,
		.ast = NULL,
		.parsed_ok = false
	};
	ctx->count++;

	if (out_fd) *out_fd = fd;
	if (out_mapped) *out_mapped = (void *)file.data;
	if (out_size) *out_size = file.length;

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
			error_add(ctx->errors, ERROR_PARSER, ERROR_LEVEL_ERROR,
					unit->file.name, 0, 0, "Parse failed");
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
				if (!semantic_global_declare_function(&ctx->global, 
							ctx->errors, func, unit->file.name))
					all_ok = false;
			}
		}
	}
	if (!all_ok)
		return (false);
	
	printf("  > analyzing function bodies\n");
	for (size_t i = 0; i < ctx->count; ++i)
	{
		CompilationUnit *unit = &ctx->units[i];
		if (!unit->parsed_ok || !unit->ast)
			continue;
		printf("%4zu | %s\n", i, unit->file.name);
		if (!semantic_analyze(ctx->arena, unit, ctx->errors, &ctx->global))
			all_ok = false;
	}

	return (all_ok);
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

	error_fatal(ctx->errors, NULL, 0, 0, "No main() function found");
	return (NULL);
}
