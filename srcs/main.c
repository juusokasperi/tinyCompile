#define MEMARENA_IMPLEMENTATION
#include "memarena.h"
#include "ast.h"
#include "ir.h"
#include "jit.h"
#include "compile.h"
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

	if (!compile_parse_all(&ctx))
	{
		fprintf(stderr, "\nParsing failed.\n");
		compile_print_errors(&ctx);
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	if (!compile_analyze_all(&ctx))
	{
		fprintf(stderr, "\nSemantic analysis failed.\n");
		compile_print_errors(&ctx);
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	printf("\nCompilation successful!\n");

	ASTNode *entry = compile_get_entry_point(&ctx);
	if (!entry)
	{
		fprintf(stderr, "No entry point found (need a 'main' function)\n");
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	printf("\n--- AST Structure ---\n");
	print_ast(entry, 0);
	printf("\n");

	IRFunction *ir = ir_gen(&jit_arena, entry);
	if (!ir)
	{
		fprintf(stderr, "IR generation failed.\n");
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}
	ir_print(ir);

	JITResult jit = jit_compile(&jit_arena, ir);
	if (!jit.code)
	{
		fprintf(stderr, "JIT compilation failed.\n");
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	printf("Generated %zu bytes of x86-64 machine code.\n", jit.size);
	if (!arena_set_prot(&jit_arena, PROT_READ | PROT_EXEC))
	{
		perror("Failed to set executable permissions");
		cleanup_files(&ctx);
		arena_free(&ast_arena);
		arena_free(&jit_arena);
		return (1);
	}

	printf("\n--- Execution ---\n");
	JITFunc func = (JITFunc)jit.code;
	int64_t result = func();
	printf("Result: %lld\n", result);

	cleanup_files(&ctx);
	arena_free(&ast_arena);
	arena_free(&jit_arena);

	return (0);
}
