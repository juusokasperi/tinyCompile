#include "ast.h"

const TypeInfo	type_info_table[] = {
	#define X_TYPE(enum_name, str_name, size, is_signed) \
		{ enum_name, str_name, size, is_signed },
	#include "types.def"
	#undef X_TYPE
};

/* Helper to parse type from string (useful for parser) */
DataType type_from_sv(StringView sv)
{
	#define X_TYPE(enum_name, str_name, size, is_signed) \
		if (sv_eq_cstr(sv, str_name)) \
			return (enum_name);
	#include "types.def"
	#undef X_TYPE
	
	return (TYPE_VOID);
}

/* Helper to check if a string is a valid type name */
bool	is_type_keyword(StringView sv)
{
	#define X_TYPE(enum_name, str_name, size, is_signed) \
		if (sv_eq_cstr(sv, str_name)) \
			return (true);
	#include "types.def"
	#undef X_TYPE

	return (false);
}
