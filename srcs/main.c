#include "memarena.h"
#include "ast.h"
#include "lexer.h"
#include "ir.h"
#include "jit.h"
#include "shared_types.h"
#include "parser.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>

static int handle_cleanup(Arena *ast_arena, Arena *jit_arena, FileMap *file)
{
		arena_free(ast_arena);
		arena_free(jit_arena);
		munmap((void *)file->data, file->length);
		return (1);
}

// Messy main just used for testing
int main(int argc, char **argv)
{
	(void) argc; // void for now
				 // also for future argv++ to skip program name and start from the file(s)
	argv[0] = "test.txt";

	FileMap file;
	if (!parse_input(argv, &file))
		return (1);
	
	print_file(&file);

	Arena ast_arena = arena_init(PROT_READ | PROT_WRITE);
	Arena jit_arena = arena_init(PROT_READ | PROT_WRITE);

	Lexer lexer;
    lexer_init(&lexer, file.data, file.length);
	
	ASTNode *root = parse(&lexer, &ast_arena);
	if (!root)
		return handle_cleanup(&ast_arena, &jit_arena, &file);
	
	printf("\n--- AST Structure ---\n");
    print_ast(root, 0);
	printf("\n");

	IRFunction *ir = ir_gen(&jit_arena, root);
	ir_print(ir);

	JITResult jit = jit_compile(&jit_arena, ir);
	if (jit.code)
	{
		printf("Generated %zu bytes of x86-64 machine code.\n", jit.size);
		if (!arena_set_prot(&jit_arena, PROT_READ | PROT_EXEC))
		{
			perror("Failed to set executable permissions");
			return handle_cleanup(&ast_arena, &jit_arena, &file);
		}
		printf("\n--- EXECUTION ---\n");
		JITFunc func = (JITFunc)jit.code;
		int64_t result = func();
		printf("Result: %lld\n", result);
	}

	handle_cleanup(&ast_arena, &jit_arena, &file);
	return (0);
}
