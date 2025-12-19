#ifndef AST_H 
# define AST_H 

# include "string_view.h"

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
	TYPE_VOID,
	TYPE_INT64,
	TYPE_CHAR,
	// TYPE_INT32,
	// TYPE_UINT32,
	// TYPE_POINTER
	// etc..
} DataType;

typedef struct {
	StringView name;
	DataType type;
} Parameter;

typedef struct ASTNode ASTNode;

struct ASTNode {
	ASTNodeType type;
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
			StringView 	var_name;
			ASTNode		*initializer;
		} var_decl;

		struct {
			StringView	var_name;
			ASTNode 	*value;
		} assignment;

		struct {
			ASTNode 	**statements;
			size_t 		count;
		} block;

		struct {
			StringView	name;
			Parameter	*params;
			size_t 		param_count;
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
