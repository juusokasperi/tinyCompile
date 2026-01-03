#include "semantic.h"
#include "ast.h"
#include "ir.h"
#include "compile.h"
#include "defines.h"
#include "error_handler.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

Scope *semantic_scope_enter(SemanticAnalyzer *sa)
{
	Scope *new_scope = arena_alloc(sa->arena, sizeof(Scope));
	new_scope->parent = sa->current;
	new_scope->var_count = 0;
	memset(new_scope->entries, 0, sizeof(new_scope->entries));
	sa->current = new_scope;
	return (new_scope);
}

void semantic_scope_exit(SemanticAnalyzer *sa)
{
	if (sa->current)
		sa->current = sa->current->parent;
}

VarInfo *semantic_scope_lookup(Scope *scope, StringView name)
{
	uint32_t	idx;
	size_t		curr;

	while (scope)
	{
		idx = hash_sv(name) & (SCOPE_HASH_SIZE - 1);
		for (size_t i = 0; i < SCOPE_HASH_SIZE; ++i)
		{
			curr = (idx + i) & (SCOPE_HASH_SIZE - 1);
			if (!scope->entries[curr].occupied)
				break;
			if (sv_eq(scope->entries[curr].info.name, name))
				return (&scope->entries[curr].info);
		}
		scope = scope->parent;
	}
	return (NULL);
}

bool semantic_scope_declare(SemanticAnalyzer *sa, StringView name, 
							DataType type, int line)
{
	uint32_t	idx;
	size_t		curr;

	Scope *scope = sa->current;
	if (!scope)
		return (false);

	idx = hash_sv(name) & (SCOPE_HASH_SIZE - 1);
	for (size_t i = 0; i < SCOPE_HASH_SIZE; ++i)
	{
		curr = (idx + i) & (SCOPE_HASH_SIZE - 1);
		if (!scope->entries[curr].occupied)
		{
			scope->entries[curr].occupied = true;
			scope->entries[curr].info = (VarInfo){
				.name = name,
				.type = type,
				.initialized = false,
				.line = line
			};
			scope->var_count++;
			return (true);
		}
		if (sv_eq(scope->entries[curr].info.name, name))
		{
			error_semantic(sa->errors, sa->filename, line, 0,
					"redeclaration of variable '%.*s' (first declared at line %d)",
					(int)name.len, name.start, scope->entries[curr].info.line);
			return (false);
		}
	}

	error_semantic(sa->errors, sa->filename, line, 0,
			"too many variables in scope (max %d)", SCOPE_HASH_SIZE);
	return (false);
}

/*
 * Currently type_compatibility function just supports void and scalar types,
 * TODO for future..
 bool check_type_compatibility(Type *dest, Type *src) {
    // 1. Resolve typedefs (unwrap alias types)
    dest = resolve_typedef(dest);
    src = resolve_typedef(src);

    // 2. Trivial Match
    if (types_are_equal(dest, src)) return true;

    // 3. Pointer Logic
    if (is_pointer(dest) && is_pointer(src)) {
        // e.g. allowing 'void *' generic assignment
        if (is_void_ptr(dest) || is_void_ptr(src)) return true;
        
        // Recursive check: int** vs int** -> check int* vs int*
        return check_type_compatibility(dest->ptr_to, src->ptr_to);
    }

    // 4. Array Decay
    if (is_pointer(dest) && is_array(src)) {
        // Check if array elements match the pointer target
        return check_type_compatibility(dest->ptr_to, src->array_of);
    }

    // 5. Numeric Promotion (your current logic)
    if (is_number(dest) && is_number(src)) {
         return check_integer_promotion(dest, src);
    }

    return false;
}
*/

static bool	check_type_compatibility(SemanticAnalyzer *sa, DataType dest, DataType src, ASTNode *node)
{
	if (dest == src)
		return (true);

	if (src == TYPE_VOID || dest == TYPE_VOID)
	{
		error_semantic(sa->errors, sa->filename, node->line, node->column,
				"invalid use of void expression");
		return (false);
	}

	if (type_is_integer(dest) && type_is_integer(src))
	{
		if (type_size(dest) >= type_size(src))
			return (true);
		error_add(sa->errors, ERROR_SEMANTIC, ERROR_LEVEL_WARNING, 
				sa->filename, node->line, node->column, 
				"implicit conversion from '%s' to '%s' may lose precision",
				type_name(src), type_name(dest));
		return (true);
	}
	error_semantic(sa->errors, sa->filename, node->line, node->column,
			"incompatible types: cannot assign '%s' to '%s'",
			type_name(src), type_name(dest));
	return (false);
}

static bool analyze_expression(SemanticAnalyzer *sa, ASTNode *node)
{
	if (!node)
		return (true);
	switch (node->type)
	{
		case AST_NUMBER:
			node->value_type = TYPE_INT64;		// TODO For now all are int64..
												// Add literal suffixes (L, UL, etc)
			return (true);
		case AST_IDENTIFIER:
		{
			VarInfo *var = semantic_scope_lookup(sa->current, node->identifier.name);
			if (!var)
			{
				error_semantic(sa->errors, sa->filename, node->line, node->column,
						"use of undeclared identifier '%.*s'",
						(int)node->identifier.name.len, node->identifier.name.start);
				return (false);
			}
			node->value_type = var->type;
			return (true);
		}
		case AST_CALL:
		{
			bool is_visible = false;
			for (size_t i = 0; i < sa->visible_count; ++i)
			{
				if (sv_eq(sa->visible_funcs[i], node->call.function_name))
				{
					is_visible = true;
					break;
				}
			}

			if (!is_visible)
			{
				error_semantic(sa->errors, sa->filename, node->line, node->column,
						"implicit declaration of function '%.*s' is invalid in tinyCompile",
						(int)node->call.function_name.len, node->call.function_name.start);
				return (false);
			}

			FunctionInfo *func = semantic_global_lookup_function(
					sa->global, node->call.function_name);
			if (!func)
			{
				error_semantic(sa->errors, sa->filename, node->line, node->column,
						"call to undefined function '%.*s'", 
						(int)node->call.function_name.len, node->call.function_name.start);
				return (false);
			}

			if (node->call.arg_count != func->param_count)
			{
				error_semantic(sa->errors, sa->filename, node->line, node->column,
						"function '%.*s' expects %zu arguments, got %zu",
						(int)node->call.function_name.len, node->call.function_name.start,
						func->param_count, node->call.arg_count);
				return (false);
			}

			bool all_ok = true;
			for (size_t i = 0; i < node->call.arg_count; ++i)
			{
				if (!analyze_expression(sa, node->call.args[i]))
					all_ok = false;
				if (!check_type_compatibility(sa, func->params[i].type,
							node->call.args[i]->value_type, node->call.args[i]))
					all_ok = false;
			}
			node->value_type = func->return_type;
			return (all_ok);
		}
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		{
			bool left_ok = analyze_expression(sa, node->binary.left);
			bool right_ok = analyze_expression(sa, node->binary.right);
			if (!left_ok || !right_ok)
				return (false);
			DataType left_type = node->binary.left->value_type;
			DataType right_type = node->binary.right->value_type;
			// TODO	Proper type promotion rules - for now, just use the larger
			if (type_size(left_type) >= type_size(right_type))
				node->value_type = left_type;
			else
				node->value_type = right_type;
			return (true);
		}
		case AST_EQUAL:
		case AST_GREATER_EQUAL:
		case AST_LESS_EQUAL:
		case AST_LESS:
		case AST_NOT_EQUAL:
		{
			bool left_ok = analyze_expression(sa, node->binary.left);
			bool right_ok = analyze_expression(sa, node->binary.right);
			node->value_type = TYPE_INT64;	// TODO Comparisons always return bool
											//		Represented as int64 for now
			return (left_ok && right_ok);
		}
		case AST_NEGATE:
		{
			bool ok = analyze_expression(sa, node->unary.operand);
			if (ok)
				node->value_type = node->unary.operand->value_type;
			return (ok);
		}
		case AST_NOT:
		{
			 bool ok = analyze_expression(sa, node->unary.operand);
			 // TODO Logical not returns TYPE_BOOL (implement later)
			 node->value_type = TYPE_INT64;
			 return (ok);
		}
		case AST_BIT_NOT:
		{
			bool ok = analyze_expression(sa, node->unary.operand);
			if (ok)
				node->value_type = node->unary.operand->value_type;
			return (ok);
		}
		case AST_BIT_AND:
		case AST_BIT_OR:
		case AST_BIT_XOR:
		case AST_LSHIFT:
		case AST_RSHIFT:
		{
			bool left_ok = analyze_expression(sa, node->binary.left);
			bool right_ok = analyze_expression(sa, node->binary.right);
			if (!left_ok || !right_ok)
				return (false);

			DataType type = node->binary.left->value_type;
			if (type_size(type) < type_size(TYPE_INT))
				type = TYPE_INT;

			node->value_type = type;
			return (true);
		}
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
			{
				init_ok = analyze_expression(sa, node->var_decl.initializer);
				if (init_ok && !check_type_compatibility(sa, node->var_decl.var_type,
							node->var_decl.initializer->value_type, node->var_decl.initializer))
					init_ok = false;
			}
			bool decl_ok = semantic_scope_declare(sa, node->var_decl.var_name,
					node->var_decl.var_type, node->line);
			return (init_ok && decl_ok);
		}
		case AST_ASSIGNMENT:
		{
			VarInfo *var = semantic_scope_lookup(sa->current, node->assignment.var_name);
			if (!var)
			{
				error_semantic(sa->errors, sa->filename, node->line, node->column,
						"assignment to undeclared variable '%.*s'",
						(int)node->assignment.var_name.len,
						node->assignment.var_name.start);
				return (false);
			}
			bool ok = analyze_expression(sa, node->assignment.value);
			node->value_type = var->type;
			if (ok && !check_type_compatibility(sa, var->type, 
						node->assignment.value->value_type, node->assignment.value))
				ok = false;
			return (ok);
		}
		case AST_RETURN:
		{
			if (node->return_stmt.expression)
			{
				if (sa->current_return_type == TYPE_VOID)
				{
					error_semantic(sa->errors, sa->filename,
							node->line, node->column,
							"void function should not return a value");
					return (false);
				}
				bool ok = analyze_expression(sa, node->return_stmt.expression);
				node->value_type = sa->current_return_type;
				if (ok && !check_type_compatibility(sa, sa->current_return_type, 
							node->return_stmt.expression->value_type, 
							node->return_stmt.expression))
					ok = false;
				return (ok);
			}
			else
			{
				if (sa->current_return_type != TYPE_VOID)
				{
					error_semantic(sa->errors, sa->filename,
							node->line, node->column,
							"non-void function must return a value");
					return (false);
				}
				return (true);
			}
		}
		case AST_IF:
		{
			bool cond_ok = analyze_expression(sa, node->if_stmt.condition);
			bool then_ok = analyze_statement(sa, node->if_stmt.then_branch);
			bool else_ok = true;
			if (node->if_stmt.else_branch)
				else_ok = analyze_statement(sa, node->if_stmt.else_branch);
			return (cond_ok && then_ok && else_ok);
		}
		case AST_WHILE:
		{
			bool cond_ok = analyze_expression(sa, node->while_stmt.condition);
			bool body_ok = analyze_statement(sa, node->while_stmt.body);
			return (cond_ok && body_ok);
		}
		case AST_BLOCK:
		{
			semantic_scope_enter(sa);
			bool all_ok = true;
			for (size_t i = 0; i < node->block.count; ++i)
			{
				if (!analyze_statement(sa, node->block.statements[i]))
					all_ok = false;
			}
			semantic_scope_exit(sa);
			return (all_ok);
		}
		default:
			return (analyze_expression(sa, node));
	}
}

bool	semantic_global_declare_function(GlobalScope *global, ErrorContext *errors, 
			ASTNode *func_node, const char *filename)
{
	if (!func_node || func_node->type != AST_FUNCTION)
		return (false);

	StringView	name = func_node->function.name;
	Parameter	*params = func_node->function.params;
	size_t		param_count = func_node->function.param_count;
	int			line = func_node->line;
	DataType	return_type = func_node->function.return_type;
	bool		is_prototype = func_node->function.is_prototype;

	if (param_count > MAX_PARAMS_PER_FUNCTION)
	{
		error_semantic(errors, filename, line, 0,
				"too many parameters (max %d)", MAX_PARAMS_PER_FUNCTION);
		return (false);
	}

	FunctionInfo *existing = semantic_global_lookup_function(global, name);
	if (existing)
	{
		if (existing->return_type != return_type)
		{
			error_semantic(errors, filename, line, 0,
					"conflicting return types for function '%.*s' "
					"(previous: %s, now: %s)",
					(int)name.len, name.start,
					type_name(existing->return_type),
					type_name(return_type));
			return (false);
		}
		if (existing->param_count != param_count)
		{
			error_semantic(errors, filename, line, 0,
					"conflicting types for function '%.*s' "
					"(previous declaration at %s:%d had %zu parameters)",
					(int)name.len, name.start, existing->filename,
					existing->line, existing->param_count);
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
				error_semantic(errors, filename, line, 0,
						"redefinition of function '%.*s' (previous definition at %s:%d)",
						(int)name.len, name.start, existing->filename, existing->line);
				return (false);
			}
			return (true);
		}
	}

	if (global->function_count >= MAX_FUNCTION_COUNT)
	{
		error_semantic(errors, filename, line, 0,
				"too many functions (max %d)", MAX_FUNCTION_COUNT);
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

FunctionInfo *semantic_global_lookup_function(GlobalScope *global, StringView name)
{
	for (size_t i = 0; i < global->function_count; ++i)
	{
		if (sv_eq(global->functions[i].name, name))
			return (&global->functions[i]);
	}
	return (NULL);
}

static bool analyze_node(SemanticAnalyzer *sa, ASTNode *node)
{
	if (node->type != AST_FUNCTION)
		return (false);
	if (node->function.is_prototype)
		return (true);
	sa->current_return_type = node->function.return_type;
	semantic_scope_enter(sa);
	bool params_ok = true;
	for (size_t i = 0; i < node->function.param_count; ++i)
	{
		Parameter *param = &node->function.params[i];
		if (!semantic_scope_declare(sa, param->name, 
					param->type, node->line))
			params_ok = false;
	}

	bool body_ok = true;
	ASTNode *body = node->function.body;
	if (body && body->type == AST_BLOCK)
	{
		for (size_t i = 0; i < body->block.count; ++i)
		{
			if (!analyze_statement(sa, body->block.statements[i]))
				body_ok = false;
		}
	}
	semantic_scope_exit(sa);
	return (params_ok && body_ok);
}

bool semantic_analyze(Arena *a, CompilationUnit *unit, ErrorContext *errors, GlobalScope *global)
{
	SemanticAnalyzer sa = {
		.arena = a,
		.errors = errors,
		.filename = unit->file.name,
		.global = global,
		.current = NULL,
		.current_return_type = TYPE_INT64,
	};

	bool all_ok = true;

	for (size_t i = 0; i < unit->ast->translation_unit.count; ++i)
	{
		ASTNode *node = unit->ast->translation_unit.declarations[i];

		if (node->type == AST_FUNCTION)
		{
			bool already_visible = false;
			for (size_t j = 0; j < sa.visible_count; ++j)
			{
				if (sv_eq(sa.visible_funcs[j], node->function.name))
				{
					already_visible = true;
					break;
				}
			}
			if (!already_visible)
			{
				if (sa.visible_count >= MAX_FUNCTION_COUNT)
				{
					error_semantic(errors, unit->file.name, node->line, 0,
							"too many functions");
					return (false);
				}
				sa.visible_funcs[sa.visible_count++] = node->function.name;
			}
		}
	}

	for (size_t i = 0; i < unit->ast->translation_unit.count; ++i)
	{
		ASTNode *node = unit->ast->translation_unit.declarations[i];
		if (!analyze_node(&sa, node))
			all_ok = false;
	}
	return (all_ok);
}
