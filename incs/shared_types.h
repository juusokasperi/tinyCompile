#ifndef SHARED_TYPES_H
# define SHARED_TYPES_H

# include <unistd.h>
# include <stdbool.h>
# include <string.h>

# define MAX_VARS 256
# define MAX_FUNCS 256
# define MAX_ARGS 16
# define MAX_CALL_SITES 1024
/* Shared types between lexer and AST */

typedef struct {
	const char *start;
	size_t len;
} StringView;

typedef struct {
	const char	*data;
	size_t 		length;
	const char	*name;
} FileMap;

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

#endif
