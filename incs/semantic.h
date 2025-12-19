#ifndef SEMANTIC_H
# define SEMANTIC_H

# include "ast.h"
# include "memarena.h"
# include "defines.h"
# include "string_view.h"
# include "error_handler.h"
# include <stdbool.h>
# include <stddef.h>

typedef struct CompilationUnit CompilationUnit;

typedef struct {
	StringView	name;
	DataType	type;
	bool		initialized;
	int			line;
} VarInfo;

typedef struct Scope Scope;

struct Scope {
	Scope	*parent;
	VarInfo	vars[MAX_VARS_PER_SCOPE];
	size_t	var_count;
};

typedef struct {
	StringView	name;
	DataType	return_type;
	Parameter	*params;
	size_t		param_count;
	int			line;
	const char	*filename;
	bool		is_prototype;
} FunctionInfo;

typedef struct {
	FunctionInfo	functions[MAX_FUNCTION_COUNT];
	size_t			function_count;
} GlobalScope;

typedef struct {
	Scope			*current;
	Arena			*arena;
	ErrorContext	*errors;
	const char		*filename;
	GlobalScope		*global;
	DataType		current_return_type;

	StringView		visible_funcs[MAX_FUNCTION_COUNT];
	size_t			visible_count;
} SemanticAnalyzer;

bool	semantic_analyze(Arena *a, CompilationUnit *unit, ErrorContext *errors, GlobalScope *global);

void	semantic_global_init(GlobalScope *global);
bool	semantic_global_declare_function(GlobalScope *global, ErrorContext *errors, 
			ASTNode *func_node, const char *filename);

FunctionInfo	*semantic_global_lookup_function(GlobalScope *global, StringView name);

Scope	*semantic_scope_enter(SemanticAnalyzer *sa);
void	semantic_scope_exit(SemanticAnalyzer *sa);
VarInfo	*semantic_scope_lookup(Scope *scope, StringView name);
bool	semantic_scope_declare(SemanticAnalyzer *sa, StringView name, DataType type, int line);

#endif
