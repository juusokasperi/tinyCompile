#include <stddef.h>
#define MEMARENA_IMPLEMENTATION
#include "memarena.h"
#include "ast.h"
#include "ir.h"
#include "jit.h"
#include "compile.h"
#include "layout.h"
#include "utils.h"
#include "defines.h"
#include "error_handler.h"
#include "cleanup.h"
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

static bool validate_arguments(int argc, char **argv, ErrorContext *errors)
{
	if (argc < 2)
	{
		error_fatal(errors, NULL, 0, 0, "Usage: %s <file1.c> [file2.c ...]", argv[0]);
		return (false);
	}
	for (int i = 1; i < argc; ++i)
	{
		size_t len = strlen(argv[i]);
		if (len < 3 || strncmp(argv[i] + len - 2, ".c", 2) != 0)
		{
			error_fatal(errors, argv[i], 0, 0, "Only .c files are supported");
			return (false);
		}
	}
	if (argc - 1 > MAX_SOURCE_FILES)
	{
		error_fatal(errors, NULL, 0, 0, "Too many files (max %d, got %d)",
				MAX_SOURCE_FILES, argc - 1);
		return (false);
	}
	return (true);
}

int main(int argc, char **argv)
{
	int	exit_code = 1;

	Arena ast_arena = arena_init(PROT_READ | PROT_WRITE);
	Arena jit_arena = arena_init(PROT_READ | PROT_WRITE);

	ErrorContext	errors;
	error_context_init(&errors, &ast_arena);
	ResourceTracker	resources;
	resource_tracker_init(&resources, argc, &ast_arena);

	print_header();
	
	if (!validate_arguments(argc, argv, &errors))
		goto cleanup;
	CompilationContext ctx;
	if (!compile_ctx_init(&ctx, &ast_arena, &errors, argc - 1))
	{
		error_fatal(&errors, NULL, 0, 0, "Failed to initialize compilation context");
		goto cleanup;
	}

	for (int i = 1; i < argc; ++i)
	{
		int		fd;
		void	*mapped;
		size_t	size;
		if (!compile_ctx_add_file(&ctx, argv[i], &resources, &fd, &mapped, &size))
		{
			error_fatal(&errors, argv[i], 0, 0, "Failed to load source file");
			goto cleanup;
		}
	}

	print_phase(1, "PARSING");
	if (!compile_parse_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > parsing failed.\n" RESET);
		goto cleanup;
	}

	print_phase(2, "SEMANTICS");
	if (!compile_analyze_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > semantic analysis failed.\n" RESET);
		goto cleanup;
	}

	if (error_has_errors(&errors))
		goto cleanup;

	print_phase(3, "JIT");
	JITContext jit_ctx;
	jit_ctx_init(&jit_ctx, &jit_arena);

	for (size_t i = 0; i < ctx.count; ++i)
	{
		CompilationUnit *unit = &ctx.units[i];
		if (!unit->parsed_ok)
			continue;

		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			if (func->function.is_prototype)
                continue;
			printf("  :: compiling symbol '%.*s'\n", (int)func->function.name.len, func->function.name.start);

			IRFunction *ir = ir_gen(&jit_arena, func);
			if (!ir)
			{
				fprintf(stderr,  BOLD_RED "  > ir generation failed\n" RESET);
				error_fatal(&errors, unit->file.name, func->line, 0,
						"IR Generation failed for function '%.*s'",
						(int)func->function.name.len, func->function.name.start);
				goto cleanup;
			}

			if (sv_eq_cstr(func->function.name, "main"))
				ir_print(ir);

			JITResult jit = jit_compile_function(&jit_ctx, ir, func);
			if (!jit.code)
			{
				fprintf(stderr, BOLD_RED "  > compilation failed\n" RESET);
				error_fatal(&errors, unit->file.name, func->line, 0,
						"JIT compilation failed for funtion '%.*s'",
						(int)func->function.name.len, func->function.name.start);
				goto cleanup;
			}
		}
	}
	
	if (!jit_link_all(&jit_ctx, &errors))
	{
		fprintf(stderr, BOLD_RED "  > linking failed\n" RESET);
		goto cleanup;
	}

	if (!arena_set_prot(&jit_arena, PROT_READ | PROT_EXEC))
	{
		perror(BOLD_RED "  > failed to set executable permissions" RESET);
		goto cleanup;
	}

	print_phase(4, "EXECUTION");
	
	bool found_main = false;
	for (size_t i = 0; i < jit_ctx.registry.count; i++)
	{
    	if (sv_eq_cstr(jit_ctx.registry.functions[i].name, "main"))
		{
        	JITFunc main_func = (JITFunc)jit_ctx.registry.functions[i].code_addr;
        	int64_t result = main_func();
			printf(GREEN "  -----------------------------------------\n");
    		printf("   RETURN CODE >> " BOLD_WHITE "%lld" RESET "\n", result);
    		printf(GREEN "  -----------------------------------------\n" RESET);
			found_main = true;
			exit_code = 0;
        	break;
    	}
	}

	if (!found_main)
	{
		error_fatal(&errors, NULL, 0, 0, "No 'main' function found.");
		goto cleanup;
	}

cleanup:
	if (error_has_errors(&errors) || error_has_fatal(&errors))
		error_print_all(&errors);
	resource_cleanup_all(&resources);
	arena_free(&ast_arena);
	arena_free(&jit_arena);

	return (exit_code);
}
