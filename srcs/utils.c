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
	out_file->name = argv[0];
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

void print_file(FileMap *file)
{
	if (!file->data)
		return;
	printf("--- File \"%s\" contents ---\n", file->name);
	size_t line = 0;
	for (size_t i = 0; i < file->length; ++i)
	{
		printf("%4zu | ", line);
		while (i < file->length && file->data[i] != '\n')
		{
			printf("%c", file->data[i]);
			i++;
		}
		line++;
		printf("\n");
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
	printf(BOX_BR RESET "\n\n");
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
