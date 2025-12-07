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
# include "shared_types.h"
# include "ast.h"

int safe_open(char *file);
FileMap map_input(int fd);
bool parse_input(char **argv, FileMap *out_file);
void print_ast(ASTNode *node, int indent);
void print_file(FileMap *file);

#endif
