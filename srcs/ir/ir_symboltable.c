#include "defines.h"
#include "ir.h"
#include "string_view.h"
#include <stddef.h>
#include <stdint.h>

static uint32_t	hash_sv(StringView sv)
{
	uint32_t	hash = 2166136261u;

	for (size_t i = 0; i < sv.len; ++i)
	{
		hash ^= sv.start[i];
		hash *= 16777619;
	}
	return (hash);
}

Symbol *symtab_lookup(SymbolTable *st, StringView name)
{
	uint32_t	hash = hash_sv(name);
	uint32_t	idx = hash & (SYMTAB_SIZE - 1);
	size_t		curr;

	for (size_t i = 0; i < SYMTAB_SIZE; ++i)
	{
		curr = (idx + i) & (SYMTAB_SIZE - 1);
		if (!st->entries[curr].occupied)
			return (NULL);
		if (sv_eq(st->entries[curr].name, name))
			return (&st->entries[curr]);
	}
	return (NULL);
}

void symtab_add(SymbolTable *st, StringView name, size_t vreg)
{
	uint32_t	hash = hash_sv(name);
	uint32_t	idx = hash & (SYMTAB_SIZE - 1);
	size_t		curr;

	for (size_t i = 0; i < SYMTAB_SIZE; ++i)
	{
		curr = (idx + i) & (SYMTAB_SIZE - 1);
		if (!st->entries[curr].occupied)
		{
			st->entries[curr].name = name;
			st->entries[curr].vreg = vreg;
			st->entries[curr].occupied = true;
			return ;
		}
		if (sv_eq(st->entries[curr].name, name))
		{
			st->entries[curr].vreg = vreg;
			return;
		}
	}
	fprintf(stderr, "Fatal: Symbol table overflow (increase SYMTAB_SIZE)\n");
	exit(1);
}
