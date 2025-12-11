#ifndef STRING_VIEW_H
# define STRING_VIEW_H

#include <string.h>
#include "memarena.h"

typedef struct {
	const char *start;
	size_t len;
} StringView;

/* StringView functions */
static inline bool sv_eq(StringView a, StringView b)
{
	if (a.len != b.len)
		return (false);
	return (memcmp(a.start, b.start, a.len) == 0);
}

static inline bool sv_eq_cstr(StringView sv, const char *str)
{
    return (strlen(str) == sv.len && memcmp(sv.start, str, sv.len) == 0);
}

static inline int64_t sv_to_int(Arena *a, StringView sv)
{
	ArenaTemp temp = arena_temp_begin(a);

    char *buf = arena_alloc(a, sv.len + 1);
    if (!buf)
	{
		arena_temp_end(temp);
		return (0);
	}
    
    memcpy(buf, sv.start, sv.len);
    buf[sv.len] = '\0';
    
	int64_t result = atoll(buf);
	arena_temp_end(temp);

    return result;
}


#endif // STRING_VIEW_H
