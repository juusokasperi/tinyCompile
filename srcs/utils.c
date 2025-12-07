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

