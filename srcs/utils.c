#include "utils.h"

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
		default:		printf("Unknown node\n"); break;
	}

	if (node->type >= AST_ADD && node->type <= AST_DIV)
	{
		print_ast(node->binary.left, indent + 1);
		print_ast(node->binary.right, indent + 1);
	}
}

