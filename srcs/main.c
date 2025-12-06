#include "memarena.h"
#include "ast.h"
#include "lexer.h"
#include "ir.h"
#include "jit.h"
#include "shared_types.h"
#include "parser.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int safe_open(char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd == -1)
	{
		perror("error opening file");
		exit(1);
	}
	return (fd);
}

FileMap map_input(int fd)
{
	FileMap res = {0};
	struct stat	stat_buf;
	if (fstat(fd, &stat_buf) == -1)
	{
		perror("fstat failed");
		close(fd);
		return (res);
	}
	
#ifdef __APPLE__
	int flags = MAP_PRIVATE;
#else
	int flags = MAP_PRIVATE | MAP_POPULATE;
#endif

	char *map = mmap(NULL, stat_buf.st_size,
			PROT_READ,
			flags,
			fd, 0);

	close(fd);
	if (map == MAP_FAILED)
	{
		perror("mmap failed");
		return (res);
	}

	res.data = map;
	res.length = stat_buf.st_size;
	return (res);
}

bool parse_input(char **argv, FileMap *out_file)
{
	// For future, multiple files parsing..
	int fd = safe_open(argv[0]);
	*out_file = map_input(fd);
	if (out_file->data)
	{
		printf("Succesfully mapped %zu bytes.\n", out_file->length);
		return (true);
	}
	return (false);
}

void print_ast(ASTNode *node, int indent)
{
	if (!node) return;
	for (int i = 0; i < indent; ++i) printf("  ");

	switch (node->type)
	{
		case AST_NUMBER:
			printf("Int: %.*s\n", (int)node->number.value.len, node->number.value.start);
			break;
		case AST_ADD:	printf("Op: +\n"); break;
		case AST_SUB:	printf("Op: -\n"); break;
		case AST_MUL:	printf("Op: *\n"); break;
		case AST_DIV:	printf("Op: /\n"); break;
		default:		printf("Unknown node\n"); break;
	}

	if (node->type >= AST_ADD && node->type <= AST_DIV)
	{
		print_ast(node->binary.left, indent + 1);
		print_ast(node->binary.right, indent + 1);
	}
}

int main(int argc, char **argv)
{
	(void) argc; // void for now
				 // also for future argv++ to skip program name and start from the file(s)
	argv[0] = "simple_test.txt";

	FileMap file;
	if (!parse_input(argv, &file))
		return (1);
	printf("File contents: \n%.*s\n", (int)file.length, file.data);

	Arena ast_arena = arena_init(PROT_READ | PROT_WRITE);
	Arena jit_arena = arena_init(PROT_READ | PROT_WRITE);

	Lexer lexer;
    lexer_init(&lexer, file.data, file.length);
	
	ASTNode *root = parse(&lexer, &ast_arena);
	
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
