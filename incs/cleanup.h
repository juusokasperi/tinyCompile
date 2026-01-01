#ifndef CLEANUP_H
# define  CLEANUP_H

# include <stdbool.h>
# include <stddef.h>
# include <sys/mman.h>
# include <unistd.h>
# include "memarena.h"

typedef struct {
	int		fd;
	void	*addr;
	size_t	size;
	bool	needs_close;
	bool	needs_munmap;
} FileResource;

typedef struct {
	FileResource	*files;
	size_t			count;
	size_t			capacity;
	Arena			*arena;
} ResourceTracker;

void	resource_tracker_init(ResourceTracker *tracker, size_t capacity, Arena *a);
bool	resource_track_fd(ResourceTracker *tracker, int fd);
bool	resource_track_mmap(ResourceTracker *tracker, void *addr, size_t size);
bool	resource_track_file(ResourceTracker *tracker, int fd, void *addr, size_t size);
void	resource_cleanup_all(ResourceTracker *tracker);
void	resource_untrack_fd(ResourceTracker *tracker, int fd);

#endif
