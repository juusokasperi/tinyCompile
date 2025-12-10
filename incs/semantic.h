#ifndef SEMANTIC_H
# define SEMANTIC_H

# include "ast.h"
# include "memarena.h"
# include "shared_types.h"
# include <stdbool.h>

typedef struct ErrorNode ErrorNode;

struct ErrorNode {
	const char	*msg;
	const char	*filename;
	int			line;
	int			column;
	ErrorNode	*next;
};

typedef struct {
	ErrorNode	*head;
	ErrorNode	*tail;
	size_t		count;
} ErrorList;

typedef struct {
	StringView	name;
	DataType	type;
	bool		initialized;
	int			line;
} VarInfo;

typedef struct Scope Scope;

struct Scope {
	Scope	*parent;
	VarInfo	vars[MAX_VARS];
	size_t	var_count;
};

typedef struct {
	StringView	name;
	DataType	return_type;
	Parameter	*params;
	size_t		param_count;
	int			line;
	const char	*filename;
} FunctionInfo;

typedef struct {
	FunctionInfo	functions[MAX_FUNCS];
	size_t			function_count;
} GlobalScope;

typedef struct {
	Scope		*current;
	Arena		*arena;
	ErrorList	*errors;
	const char	*filename;
	GlobalScope	*global;
} SemanticAnalyzer;

bool	semantic_analyze(Arena *a, ASTNode *root, ErrorList *errors, const char *filename, GlobalScope *global);

void	global_scope_init(GlobalScope *globa);
bool	global_declare_function(GlobalScope *global, Arena *a, ErrorList *errors,
                ASTNode *func_node, const char *filename);

FunctionInfo	*global_lookup_function(GlobalScope *global, StringView name);

void	error_list_init(ErrorList *list);
void	error_list_add(ErrorList *list, Arena *a, const char *msg, const char *filename, int line, int col);
void	error_list_print(ErrorList *list);

Scope	*scope_enter(SemanticAnalyzer *sa);
void	scope_exit(SemanticAnalyzer *sa);
VarInfo	*scope_lookup(Scope *scope, StringView name);
bool	scope_declare(SemanticAnalyzer *sa, StringView name, DataType type, int line);

#endif
