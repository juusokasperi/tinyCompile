#ifndef FILE_MAP_H
# define FILE_MAP_H

# include <stddef.h>

typedef struct {
	const char	*data;
	size_t 		length;
	const char	*name;
} FileMap;

FileMap map_input(int fd);
int safe_open(char *file);

#endif // FILE_MAP_H
