#define MEMARENA_IMPLEMENTATION
#include "memarena.h"
#include "ast.h"
#include "ir.h"
#include "jit.h"
#include "compile.h"
#include "layout.h"
#include "utils.h"
#include <stdio.h>
#include <sys/mman.h>

static void cleanup_files(CompilationContext *ctx)
{
	for (size_t i = 0; i < ctx->count; ++i)
	{
		if (ctx->units[i].file.data)
			munmap((void *)ctx->units[i].file.data, ctx->units[i].file.length);
	}
}

int main(int argc, char **argv)
{
	print_header();
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <file1.c> [file2.c ...]\n", argv[0]);
		return (1);
	}

	Arena ast_arena = arena_init(PROT_READ | PROT_WRITE);
	Arena jit_arena = arena_init(PROT_READ | PROT_WRITE);

	CompilationContext ctx;
	compile_ctx_init(&ctx, &ast_arena, argc - 1);

	for (int i = 1; i < argc; ++i)
	{
		if (!compile_ctx_add_file(&ctx, argv[i]))
		{
			cleanup_files(&ctx);
			arena_free(&ast_arena);
			arena_free(&jit_arena);
			return (1);
		}
	}

	print_phase(1, "PARSING");
	if (!compile_parse_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > parsing failed.\n" RESET);
		compile_print_errors(&ctx);
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	print_phase(2, "SEMANTICS");
	if (!compile_analyze_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > semantic analysis failed.\n" RESET);
		compile_print_errors(&ctx);
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	JITContext jit_ctx;
	jit_ctx_init(&jit_ctx, &jit_arena);

	print_phase(3, "JIT");
	for (size_t i = 0; i < ctx.count; ++i)
	{
		CompilationUnit *unit = &ctx.units[i];
		if (!unit->parsed_ok)
			continue;

		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			printf("  :: compiling symbol '%.*s'\n", (int)func->function.name.len, func->function.name.start);

			IRFunction *ir = ir_gen(&jit_arena, func);
			if (!ir)
			{
				fprintf(stderr,  BOLD_RED "  > ir generation failed\n" RESET);
				cleanup_files(&ctx);
				arena_free(&ast_arena);
				arena_free(&jit_arena);
				return (1);
			}

			if (sv_eq_cstr(func->function.name, "main"))
				ir_print(ir);

			JITResult jit = jit_compile_function(&jit_ctx, ir, func);
			if (!jit.code)
			{
				fprintf(stderr, BOLD_RED "  > compilation failed\n" RESET);
				cleanup_files(&ctx);
				arena_free(&ast_arena);
				arena_free(&jit_arena);
				return (1);
			}
		}
	}
	
	if (!jit_link_all(&jit_ctx))
	{
		fprintf(stderr, BOLD_RED "  > linking failed\n" RESET);
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	if (!arena_set_prot(&jit_arena, PROT_READ | PROT_EXEC))
	{
		perror(BOLD_RED "  > failed to set executable permissions" RESET);
		printf("\n");
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	print_phase(4, "EXECUTION");
	
	for (size_t i = 0; i < jit_ctx.registry.count; i++)
	{
    	if (sv_eq_cstr(jit_ctx.registry.functions[i].name, "main"))
		{
        	JITFunc main_func = (JITFunc)jit_ctx.registry.functions[i].code_addr;
        	int64_t result = main_func();
			printf(GREEN "  -----------------------------------------\n");
    		printf("   RETURN CODE >> " BOLD_WHITE "%lld" RESET "\n", result);
    		printf(GREEN "  -----------------------------------------\n" RESET);
        	break;
    	}
	}

	cleanup_files(&ctx);
	arena_free(&ast_arena);
	arena_free(&jit_arena);

	return (0);
}
