#include "cleanup.h"
#include <stdlib.h>
#include <string.h>

void	resource_tracker_init(ResourceTracker *tracker, size_t capacity, Arena *a)
{
	tracker->capacity = capacity;
	tracker->count = 0;
	tracker->arena = a;
	tracker->files = arena_alloc(tracker->arena, capacity * sizeof(FileResource));
}

void	resource_track_fd(ResourceTracker *tracker, int fd)
{
	// TODO improve silent failure
	if (tracker->count >= tracker->capacity)
		return;
	tracker->files[tracker->count++] = (FileResource){
		.fd = fd,
		.needs_close = true,
		.needs_munmap = false,
	};
}

void	resource_track_mmap(ResourceTracker *tracker, void *addr, size_t size)
{
	// TODO improve silent failure
	if (tracker->count >= tracker->capacity)
		return;
	tracker->files[tracker->count++] = (FileResource){
		.fd = -1,
		.addr = addr,
		.size = size,
		.needs_close = false,
		.needs_munmap = true,
	};
}

void	resource_track_file(ResourceTracker *tracker, int fd, void *addr, size_t size)
{
	// TODO improve silent failure
	if (tracker->count >= tracker->capacity)
		return;
	tracker->files[tracker->count++] = (FileResource){
		.fd = fd,
		.addr = addr,
		.size = size,
		.needs_close = true,
		.needs_munmap = true,
	};
}

void	resource_cleanup_all(ResourceTracker *tracker)
{
	for (size_t i = 0; i < tracker->count; ++i)
	{
		FileResource *res = &tracker->files[i];
		if (res->needs_munmap && res->addr)
			munmap(res->addr, res->size);
		if (res->needs_close && res->fd >= 0)
			close(res->fd);
	}
	tracker->files = NULL;
	tracker->count = 0;
}

void	resource_untrack_fd(ResourceTracker *tracker, int fd)
{
	for (size_t i = 0; i < tracker->count; ++i)
	{
		if (tracker->files[i].fd == fd)
		{
			tracker->files[i].needs_close = false;
			return;
		}
	}
}
