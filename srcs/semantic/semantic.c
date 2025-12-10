#include "semantic.h"
#include <stdio.h>

void error_list_init(ErrorList *list)
{
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

void error_list_add(ErrorList *list, Arena *a, const char *msg, const char *filename, int line, int col)
{
	ErrorNode *node = arena_alloc(a, sizeof(ErrorNode));
	node->msg = msg;
	node->filename = filename;
	node->line = line;
	node->column = col;
	node->next = NULL;

	if (list->tail)
	{
		list->tail->next = node;
		list->tail = node;
	}
	else
		list->head = list->tail = node;
	list->count++;
}

void error_list_print(ErrorList *list)
{
	ErrorNode *curr = list->head;
	while (curr)
	{
		fprintf(stderr, "%s:%d:%d: error: %s\n",
				curr->filename,
				curr->line,
				curr->column,
				curr->msg);
		curr = curr->next;
	}
}

Scope *scope_enter(SemanticAnalyzer *sa)
{
	Scope *new_scope = arena_alloc(sa->arena, sizeof(Scope));
	new_scope->parent = sa->current;
	new_scope->var_count = 0;
	sa->current = new_scope;
	return (new_scope);
}

void scope_exit(SemanticAnalyzer *sa)
{
	if (sa->current)
		sa->current = sa->current->parent;
}

VarInfo *scope_lookup(Scope *scope, StringView name)
{
	// printf("DEBUG: Looking up '%.*s' (starting scope=%p)\n", 
    //   (int)name.len, name.start, (void*)scope);
	while (scope)
	{
		// printf("  Checking scope %p with %zu vars\n", (void*)scope, scope->var_count);
		for (size_t i = 0; i < scope->var_count; ++i)
		{
			if (sv_eq(scope->vars[i].name, name))
				return (&scope->vars[i]);
		}
		scope = scope->parent;
	}
    // printf("  Not found!\n");
	return (NULL);
}

bool scope_declare(SemanticAnalyzer *sa, StringView name, DataType type, int line)
{
	Scope *scope = sa->current;
	if (!scope)
		return (false);
	// printf("DEBUG: Declaring '%.*s' at line %d (scope=%p)\n", 
    //	(int)name.len, name.start, line, (void*)scope);

	for (size_t i = 0; i < scope->var_count; ++i)
	{
		if (sv_eq(scope->vars[i].name, name))
		{
			char *msg = arena_sprintf(sa->arena,
				"variable '%.*s' already declared in this scope (first declared at line %d)",
				(int)name.len, name.start, scope->vars[i].line);
			error_list_add(sa->errors, sa->arena, msg, sa->filename, line, 0);
			return (false);
		}
	}

	if (scope->var_count >= 256)
	{
		error_list_add(sa->errors, sa->arena, "too many variables in scope", sa->filename, line, 0);
		return (false);
	}

	scope->vars[scope->var_count] = (VarInfo){
		.name = name,
		.type = type,
		.initialized = false,
		.line = line
	};
	scope->var_count++;
	return (true);
}

static bool analyze_expression(SemanticAnalyzer *sa, ASTNode *node)
{
	if (!node)
		return (true);
	switch (node->type)
	{
		case AST_NUMBER:
			return (true);
		case AST_IDENTIFIER:
		{
			VarInfo *var = scope_lookup(sa->current, node->identifier.name);
			if (!var)
			{
				char *msg = arena_sprintf(sa->arena,
					"undefined variable '%.*s'",
					(int)node->identifier.name.len, node->identifier.name.start);
				error_list_add(sa->errors, sa->arena, msg, sa->filename, node->line, node->column);
				return (false);
			}
			return (true);
		}
		case AST_CALL:
		{
			FunctionInfo *func = global_lookup_function(sa->global, node->call.function_name);
			if (!func)
			{
				char *msg = arena_sprintf(sa->arena, "call to undefined function '%.*s'", (int)node->call.function_name.len, node->call.function_name.start);
				error_list_add(sa->errors, sa->arena, msg, sa->filename, node->line, node->column);
				return (false);
			}

			if (node->call.arg_count != func->param_count)
			{
				char *msg = arena_sprintf(sa->arena, "function '%.*s' expects %zu arguments, got %zu",
						(int)node->call.function_name.len, node->call.function_name.start, func->param_count, node->call.arg_count);
				error_list_add(sa->errors, sa->arena, msg, sa->filename, node->line, node->column);
				return (false);
			}

			bool all_ok = true;
			for (size_t i = 0; i < node->call.arg_count; ++i)
			{
				if (!analyze_expression(sa, node->call.args[i]))
					all_ok = false;
			}
			return (all_ok);
		}
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		{
			bool left_ok = analyze_expression(sa, node->binary.left);
			bool right_ok = analyze_expression(sa, node->binary.right);
			return (left_ok && right_ok);
		}
		case AST_NEGATE:
		case AST_NOT:
			return (analyze_expression(sa, node->unary.operand));
		default:
			return (true);
	}
}

static bool analyze_statement(SemanticAnalyzer *sa, ASTNode *node)
{
	if (!node)
		return (true);

	switch (node->type)
	{
		case AST_VAR_DECL:
		{
			bool init_ok = true;
			if (node->var_decl.initializer)
				init_ok = analyze_expression(sa, node->var_decl.initializer);
			bool decl_ok = scope_declare(sa, node->var_decl.var_name, TYPE_INT32, node->line);
			return (init_ok && decl_ok);
		}
		case AST_ASSIGNMENT:
		{
			VarInfo *var = scope_lookup(sa->current, node->assignment.var_name);
			if (!var)
			{
				char *msg = arena_sprintf(sa->arena,
					"assignment to undefined variable '%.*s'",
					(int)node->assignment.var_name.len, node->assignment.var_name.start);
				error_list_add(sa->errors, sa->arena, msg, sa->filename, node->line, node->column);
				return (false);
			}
			return (analyze_expression(sa, node->assignment.value));
		}
		case AST_RETURN:
		{
			if (node->return_stmt.expression)
			{
				if (sa->current_return_type == TYPE_VOID)
				{
					error_list_add(sa->errors, sa->arena, "void function should not return a value",
							sa->filename, node->line, node->column);
					return (false);
				}
				return (analyze_expression(sa, node->return_stmt.expression));
			}
			else
			{
				if (sa->current_return_type != TYPE_VOID)
				{
					error_list_add(sa->errors, sa->arena, "non-void function must return a value",
							sa->filename, node->line, node->column);
					return (false);
				}
				return (true);
			}
		}
		case AST_BLOCK:
		{
			scope_enter(sa);
			bool all_ok = true;
			for (size_t i = 0; i < node->block.count; ++i)
			{
				if (!analyze_statement(sa, node->block.statements[i]))
					all_ok = false;
			}
			scope_exit(sa);
			return (all_ok);
		}
		default:
			return (analyze_expression(sa, node));
	}
}

void global_scope_init(GlobalScope *global)
{
	global->function_count = 0;
}

bool	global_declare_function(GlobalScope *global, Arena *a, ErrorList *errors,
                				ASTNode *func_node, const char *filename)
{
	if (!func_node || func_node->type != AST_FUNCTION)
		return (false);

	StringView name = func_node->function.name;
	Parameter *params = func_node->function.params;
	size_t param_count = func_node->function.param_count;
	int line = func_node->line;
	DataType return_type = TYPE_INT32;
	bool is_prototype = func_node->function.is_prototype;

	FunctionInfo *existing = global_lookup_function(global, name);
	if (existing)
	{
		// Check if param count matches (naive for now)
		if (existing->param_count != param_count)
		{
			char *msg = arena_sprintf(a, "conflicting types for '%.*s'",
					(int)name.len, name.start);
			error_list_add(errors, a, msg, filename, line, 0);
			return (false);
		}

		// Path 1: Existing function is a prototype
		if (existing->is_prototype)
		{
			// If new function is not prototype, update the existing
			// Else, it's a redeclaration
			if (!is_prototype)
			{
				existing->is_prototype = false;
				existing->line = line;
				existing->filename = filename;
				existing->params = params;
				return (true);
			}
			return (true);
		}

		// Path 2: Existing is a definition
		else
		{
			// If new one is also definition, it is an error
			if (!is_prototype)
			{
				char *msg = arena_sprintf(a, "redefinition of '%.*s' (previous definition was at %s:%d)",
						(int)name.len, name.start, existing->filename, existing->line);
				error_list_add(errors, a, msg, filename, line, 0);
				return (false);
			}
			return (true);
		}
	}

	if (global->function_count >= MAX_FUNCS)
	{
		error_list_add(errors, a, "too many functions", filename, line, 0);
		return (false);
	}

	global->functions[global->function_count] = (FunctionInfo){
		.name = name,
		.return_type = return_type,
		.params = params,
		.param_count = param_count,
		.line = line,
		.filename = filename,
		.is_prototype = is_prototype
	};
	global->function_count++;

	return (true);
}

FunctionInfo *global_lookup_function(GlobalScope *global, StringView name)
{
	for (size_t i = 0; i < global->function_count; ++i)
	{
		if (sv_eq(global->functions[i].name, name))
			return (&global->functions[i]);
	}
	return (NULL);
}

bool semantic_analyze(Arena *a, ASTNode *root, ErrorList *errors, 
		const char *filename, GlobalScope *global)
{
	if (!root)
		return (false);
	
	SemanticAnalyzer sa = {
		.arena = a,
		.errors = errors,
		.filename = filename,
		.global = global,
		.current = NULL,
		.current_return_type = TYPE_INT32,
	};

	scope_enter(&sa);
	bool result = true;
	if (root->type == AST_FUNCTION)
	{
		for (size_t i = 0; i < root->function.param_count; ++i)
		{
			Parameter *param = &root->function.params[i];
			if (!scope_declare(&sa, param->name, param->type, root->line))
				result = false;
		}
		ASTNode *body = root->function.body;
		if (body && body->type == AST_BLOCK)
		{
			for (size_t i = 0; i < body->block.count; ++i)
			{
				if (!analyze_statement(&sa, body->block.statements[i]))
					result = false;
			}
		}
	}
	else if (root->type == AST_BLOCK)
	{
		for (size_t i = 0; i < root->block.count; ++i)
		{
			if (!analyze_statement(&sa, root->block.statements[i]))
				result = false;
		}
	}
	else
		result = analyze_expression(&sa, root);

	scope_exit(&sa);
	return (result && (errors->count == 0));
}
