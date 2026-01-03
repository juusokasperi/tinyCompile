#ifndef AST_H 
# define AST_H 

# include "string_view.h"
# include <stdbool.h>
# include <stddef.h>

typedef enum {
	AST_NUMBER,
	AST_IDENTIFIER,

	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,

	AST_NEGATE,		// -x
	AST_NOT,		// !x

	AST_EQUAL,
	AST_NOT_EQUAL,
	AST_LESS,
	AST_LESS_EQUAL,
	AST_GREATER,
	AST_GREATER_EQUAL,

	AST_BIT_AND,
	AST_BIT_OR,
	AST_BIT_XOR,
	AST_BIT_NOT,
	AST_LSHIFT,
	AST_RSHIFT,

	AST_VAR_DECL,
	AST_ASSIGNMENT,
	AST_IF,
	AST_WHILE,
	AST_BLOCK,
	AST_RETURN,

	AST_FUNCTION,
	AST_TRANSLATION_UNIT,
	AST_CALL,
} ASTNodeType;

typedef enum {
	#define X_TYPE(enum_name, str_name, size, is_signed) enum_name,
	#include "types.def"
	#undef X_TYPE
} DataType;

typedef struct {
	DataType	type;
	const char	*name;
	size_t		size;
	bool		is_signed;
} TypeInfo;

/* Global type information table */
extern const TypeInfo type_info_table[];

/* Helper functions using the type table */
static inline size_t type_size(DataType type)
{
	return (type_info_table[type].size);
}

static inline bool type_is_signed(DataType type)
{
	return (type_info_table[type].is_signed);
}

static inline const char* type_name(DataType type)
{
	return (type_info_table[type].name);
}

/* Helper to check if type is integer */
static inline bool type_is_integer(DataType type)
{
	return (type != TYPE_VOID);  // For now, everything except void is an integer
}

/* Helper to check if type is unsigned */
static inline bool type_is_unsigned(DataType type)
{
	return (!type_is_signed(type) && type != TYPE_VOID);
}

/* Helper to get unsigned version of a signed type */
static inline DataType type_to_unsigned(DataType type)
{
	switch (type)
	{
		case TYPE_INT64:	return (TYPE_UINT64);
		case TYPE_INT32:	return (TYPE_UINT32);
		case TYPE_INT16:	return (TYPE_UINT16);
		case TYPE_INT8:		return (TYPE_UINT8);
		case TYPE_CHAR:		return (TYPE_UINT8);
		default:			return (type);
	}
}

/* Helper to get signed version of an unsigned type */
static inline DataType type_to_signed(DataType type)
{
	switch (type)
	{
		case TYPE_UINT64:	return (TYPE_INT64);
		case TYPE_UINT32:	return (TYPE_INT32);
		case TYPE_UINT16:	return (TYPE_INT16);
		case TYPE_UINT8:	return (TYPE_INT8);
		default:			return (type);
	}
}

/* Helper to get larger of two types (for promotion) */
static inline DataType type_promote(DataType a, DataType b)
{
	// If either is void, return the other
	if (a == TYPE_VOID)
		return (b);
	if (b == TYPE_VOID) 
		return (a);
	
	// Promote to larger size
	size_t size_a = type_size(a);
	size_t size_b = type_size(b);
	
	if (size_a > size_b)
		return (a);
	if (size_b > size_a)
		return (b);
	
	// Same size - prefer unsigned
	if (type_is_unsigned(a)) 
		return (a);
	return (b);
}

/* Helper functions for type parsing (defined in ast.c) */
DataType	type_from_sv(StringView sv);
bool		is_type_keyword(StringView sv);

typedef struct {
	StringView name;
	DataType type;
} Parameter;

typedef struct ASTNode ASTNode;

struct ASTNode {
	ASTNodeType type;
	DataType	value_type;
	int line, column;

	union
	{
		struct
		{
			StringView value;
		} number;

		struct
		{
			StringView name;
		} identifier;

		struct
		{ 
			ASTNode *left;
			ASTNode *right;
		} binary;

		struct {
			ASTNode *operand;
		} unary;

		struct
		{
			ASTNode	*condition;
			ASTNode *then_branch;
			ASTNode *else_branch; // NULL if no else 
		} if_stmt;

		struct {
			ASTNode *condition;
			ASTNode *body;
		} while_stmt;

		struct {
			ASTNode *expression;
		} return_stmt;

		struct {
			StringView	var_name;
			DataType	var_type;
			ASTNode		*initializer;
		} var_decl;

		struct {
			StringView	var_name;
			ASTNode		*value;
		} assignment;

		struct {
			ASTNode		**statements;
			size_t		count;
		} block;

		struct {
			StringView	name;
			DataType	return_type;
			Parameter	*params;
			size_t		param_count;
			ASTNode		*body;
			bool		is_prototype;
		} function;

		struct {
			ASTNode **declarations;
			size_t	count;
		} translation_unit;

		struct {
			StringView function_name;
			ASTNode **args;
			size_t arg_count;
		} call;
	};
};

void	print_ast(ASTNode *node, int indent);

#endif
