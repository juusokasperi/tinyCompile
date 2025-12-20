#include "utils.h"
#include "layout.h"
#include "file_map.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "ast.h"

int safe_open(char *file)
{
	int fd = open(file, O_RDONLY);
	if (fd == -1)
	{
		perror(BOLD_RED "  > error opening file" RESET);
		printf("\n");
	}
	return (fd);
}

FileMap map_input(int fd)
{
	FileMap res = {0};
	struct stat	stat_buf;
	if (fstat(fd, &stat_buf) == -1)
	{
		perror(BOLD_RED "  > fstat failed" RESET);
		printf("\n");
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
		perror(BOLD_RED "  > mmap failed" RESET);
		printf("\n");
		return (res);
	}

	res.data = map;
	res.length = stat_buf.st_size;
	return (res);
}

void print_ast(ASTNode *node, int indent)
{
	if (!node) return;
	for (int i = 0; i < indent; ++i) printf("  ");

	switch (node->type)
	{
		case AST_FUNCTION:
            printf("Function: %.*s\n", (int)node->function.name.len, node->function.name.start);
            print_ast(node->function.body, indent + 1);
            break;
		case AST_BLOCK:
            printf("Block\n");
            for (size_t i = 0; i < node->block.count; ++i)
                print_ast(node->block.statements[i], indent + 1);
            break;
		case AST_VAR_DECL:
            printf("VarDecl: %.*s\n", (int)node->var_decl.var_name.len, node->var_decl.var_name.start);
            if (node->var_decl.initializer)
                print_ast(node->var_decl.initializer, indent + 2);
            break;
        case AST_ASSIGNMENT:
            printf("Assign: %.*s\n", (int)node->assignment.var_name.len, node->assignment.var_name.start);
            print_ast(node->assignment.value, indent + 1);
            break;
        case AST_RETURN:
            printf("Return\n");
            print_ast(node->return_stmt.expression, indent + 1);
            break;
        case AST_IDENTIFIER:
            printf("ID: %.*s\n", (int)node->identifier.name.len, node->identifier.name.start);
            break;
		case AST_NUMBER:
			printf("Int: %.*s\n", (int)node->number.value.len, node->number.value.start);
			break;
		case AST_ADD:	printf("Op: +\n"); break;
		case AST_SUB:	printf("Op: -\n"); break;
		case AST_MUL:	printf("Op: *\n"); break;
		case AST_DIV:	printf("Op: /\n"); break;
		case AST_CALL:
    		printf("Call: %.*s(", (int)node->call.function_name.len, node->call.function_name.start);
    		for (size_t i = 0; i < node->call.arg_count; i++)
			{
        		if (i > 0) printf(", ");
        		printf("arg%zu", i);
    		}
    		printf(")\n");
    		for (size_t i = 0; i < node->call.arg_count; i++) {
   		    	print_ast(node->call.args[i], indent + 1);
  			}
    		break;
		default:		printf("Unknown node\n"); break;
	}

	if (node->type >= AST_ADD && node->type <= AST_DIV)
	{
		print_ast(node->binary.left, indent + 1);
		print_ast(node->binary.right, indent + 1);
	}
}

void print_header(void)
{
	const char *logo = RESET BOLD GREEN
	BOX_V RESET CYAN "  _   _                                   " RESET BOLD GREEN BOX_V "\n"       
	BOX_V RESET CYAN " | |_(_)_ __  _   _                       " RESET BOLD GREEN BOX_V "\n" 
	BOX_V RESET CYAN " | __| | '_ \\| | | |  " BOLD "tinyCompile v0.1    " RESET BOLD GREEN BOX_V "\n"
	BOX_V RESET CYAN " | |_| | | | | |_| |  " BOLD "Build: release      " RESET BOLD GREEN BOX_V "\n"
	BOX_V RESET CYAN "  \\__|_|_| |_|\\__, |                      " RESET BOLD GREEN BOX_V "\n"
	BOX_V RESET CYAN "              |___/                       " RESET BOLD GREEN BOX_V "\n";
	
	printf(BOLD GREEN BOX_TL);
	for (int i = 0; i < 42; ++i)
		printf(BOX_H);
	printf(BOX_TR "\n");
	printf("%s", logo);
	printf(BOX_BL);
	for (int i = 0; i < 42; ++i)
		printf(BOX_H);
	printf(BOX_BR RESET "\n");
}

void print_phase(int step, const char *name)
{
	const int WIDTH = 42;

	if (step < 0)
		printf("\n" BOLD_RED "[KO] %s" RESET, name);
	else
		printf("\n" BOLD_GREEN "[%02d] %s" RESET, step, name);

	int len = strlen(name) + 4;
	int dots = WIDTH - len;
	if (step < 0)
		printf(DIM RED);
	else
		printf(DIM GREEN);
	for (int i = 0; i < dots; ++i)
		printf(".");
	printf(RESET "\n");
}
