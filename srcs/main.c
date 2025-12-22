#include <stddef.h>
#define MEMARENA_IMPLEMENTATION
#include "memarena.h"
#include "jit.h"
#include "compile.h"
#include "layout.h"
#include "utils.h"
#include "error_handler.h"
#include "cleanup.h"
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

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
	
	print_phase(1, "INITIALIZATION");
	CompilationContext ctx;
	if (!compile_ctx_init(&ctx, &ast_arena, &errors, argc - 1))
	{
		fprintf(stderr, BOLD_RED "\n  > initialization failed\n" RESET);
		goto cleanup;
	}

	for (int i = 1; i < argc; ++i)
	{
		if (!compile_ctx_add_file(&ctx, argv[i], &resources))
		{
			fprintf(stderr, BOLD_RED "\n  > initialization failed\n" RESET);
			goto cleanup;
		}
	}

	print_phase(2, "PARSING");
	if (!compile_parse_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > parsing failed\n" RESET);
		goto cleanup;
	}

	print_phase(3, "SEMANTICS");
	if (!compile_analyze_all(&ctx))
	{
		fprintf(stderr, BOLD_RED "\n  > semantic analysis failed\n" RESET);
		goto cleanup;
	}

	print_phase(4, "JIT");
	JITContext jit_ctx;
	jit_ctx_init(&jit_ctx, &jit_arena);
	if (!jit_compile_pass(&jit_ctx, &ctx, &jit_arena, &errors))
		goto cleanup;

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

	print_phase(5, "EXECUTION");
	
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
