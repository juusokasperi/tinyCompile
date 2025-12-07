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

// Messy main just used for testing
int main(int argc, char **argv)
{
	(void) argc; // void for now
				 // also for future argv++ to skip program name and start from the file(s)
	argv[0] = "test.txt";

	FileMap file;
	if (!parse_input(argv, &file))
		return (1);
	printf("File contents: \n%.*s\n", (int)file.length, file.data);

	Arena ast_arena = arena_init(PROT_READ | PROT_WRITE);
	Arena jit_arena = arena_init(PROT_READ | PROT_WRITE);

	Lexer lexer;
    lexer_init(&lexer, file.data, file.length);
	
	ASTNode *root = parse(&lexer, &ast_arena);
	if (!root)
		return (1);
	
	printf("--- AST Structure ---\n");
    print_ast(root, 0);

	IRFunction *ir = ir_gen(&jit_arena, root);
	ir_print(ir);

	JITResult jit = jit_compile(&jit_arena, ir);
	if (jit.code)
	{
		printf("Generated %zu bytes of x86-64 machine code.\n", jit.size);
		if (!arena_set_prot(&jit_arena, PROT_READ | PROT_EXEC))
		{
			perror("Failed to set executable permissions");
			return (1);
		}
		printf("\n--- EXECUTION ---\n");
		JITFunc func = (JITFunc)jit.code;
		int64_t result = func();
		printf("Result: %lld\n", result);
	}

	munmap((void *)file.data, file.length);
    arena_free(&ast_arena);
	arena_free(&jit_arena);
	return (0);
}
