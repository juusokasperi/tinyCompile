#include "jit.h"

Symbol *symtab_lookup(SymbolTable *st, StringView name)
{
	for (size_t i = 0; i < st->count; ++i)
	{
		if (sv_eq(st->symbols[i].name, name))
			return (&st->symbols[i]);
	}
	return (NULL);
}

void symtab_add(SymbolTable *st, StringView name, size_t vreg)
{
	st->symbols[st->count] = (Symbol){.name = name, .vreg = vreg};
	st->count++;
}
