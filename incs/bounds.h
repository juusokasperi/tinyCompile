#ifndef BOUNDS_H
# define BOUNDS_H

# include <stddef.h>
# include <stdbool.h>
# include <stdio.h>
# include <stdlib.h>

static inline bool bounds_check(size_t index, size_t capacity, const char *array_name)
{
	if (index >= capacity)
	{
		fprintf(stderr, "Array bounds violation: %s[%zu] (capacity: %zu)\n",
				array_name, index, capacity);
		return (false);
	}
	return (true);
}

#define SAFE_INDEX(array, index, capacity) \
    (bounds_check((index), (capacity), #array) ? &(array)[index] : NULL)

static inline bool safe_increment(size_t *value, size_t limit, const char *name)
{
    if (*value >= limit - 1) {
        fprintf(stderr, "Counter overflow: %s (limit: %zu)\n", name, limit);
        return false;
    }
    (*value)++;
    return true;
}

static inline bool safe_add(size_t *dest, size_t value, size_t limit, const char *name)
{
    if (value > limit || *dest > limit - value) {
        fprintf(stderr, "Addition overflow: %s (limit: %zu)\n", name, limit);
        return false;
    }
    *dest += value;
    return true;
}

#ifdef DEBUG_BOUNDS
#define BOUNDS_INFO(array, index, capacity) \
    fprintf(stderr, "[BOUNDS] %s[%zu/%zu] at %s:%d\n", \
            #array, (size_t)(index), (size_t)(capacity), __FILE__, __LINE__)
#else
#define BOUNDS_INFO(array, index, capacity) ((void)0)
#endif

#endif // BOUNDS_H
