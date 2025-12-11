#ifndef UTILS_H
# define UTILS_H

# include <unistd.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include "ast.h"
# include "layout.h"

typedef struct {
	const char	*data;
	size_t 		length;
	const char	*name;
} FileMap;

int safe_open(char *file);
FileMap map_input(int fd);
void print_ast(ASTNode *node, int indent);
void print_header(void);
void print_phase(int step, const char *name);

#endif
