#include "ast.h"
#include "error_handler.h"
#include "ir.h"
#include <stddef.h>
#include <stdlib.h>

static size_t	gen_expression(Arena *a, IRFunction *f,
		ASTNode *node, SymbolTable *symbol_table);
static void		gen_statement(Arena *a, IRFunction *f, 
		ASTNode *node, SymbolTable *symbol_table, size_t *last_reg);


static void emit(Arena *a, IRFunction *f, IRInstruction inst)
{
	if (f->tail == NULL || f->tail->count >= IR_CHUNK_SIZE)
	{
		IRChunk *new_chunk = arena_alloc(a, sizeof(IRChunk));
		*new_chunk = (IRChunk){0};
		if (f->tail)
			f->tail->next = new_chunk;
		else
			f->head = new_chunk;
		f->tail = new_chunk;
	}

	f->tail->instructions[f->tail->count] = inst;
	f->tail->count++;
	f->total_count++;
}

static size_t gen_number(Arena *a, IRFunction *f, ASTNode *node)
{
	size_t reg;
	if (!ir_alloc_vreg(f, &reg))
		return (0);

	int64_t val = sv_to_int(a, node->number.value);
	IRInstruction inst = { 
		.opcode = IR_CONST, 
		.type = node->value_type,
		.dest = reg, 
		.imm = val
	};
	emit(a, f, inst);
	return (reg);
}

static size_t gen_identifier(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	Symbol *sym = symbol_table_lookup(symbol_table, node->identifier.name);
	if (!sym)
	{
		error_add(f->errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR,
				f->filename, node->line, node->column,
				"undefined variable '%.*s'",
				(int)node->identifier.name.len, node->identifier.name.start);
		return (0);
	}
	if (sym->is_stack)
	{
		size_t	temp_reg;
		if (!ir_alloc_vreg(f, &temp_reg))
			return (0);
		IRInstruction load = {
			.opcode = IR_LOAD,
			.type = node->value_type,
			.dest = temp_reg,
			.src_1 = sym->index
		};
		emit(a, f, load);
		return (temp_reg);
	}
	return (sym->index);
}

static size_t gen_call(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	size_t	arg_vregs[MAX_PARAMS_PER_FUNCTION];
	size_t	result_reg;

	for (size_t i = 0; i < node->call.arg_count; ++i)
	{
		arg_vregs[i] = gen_expression(a, f, node->call.args[i], symbol_table);
		if (arg_vregs[i] == 0)
			return (0);
	}
	for (size_t i = 0; i < node->call.arg_count; ++i)
	{
		IRInstruction arg_inst = { 
			.opcode = IR_ARG,
			.type = node->call.args[i]->value_type,
			.src_1 = arg_vregs[i],
			.imm = i
		};
		emit(a, f, arg_inst);
	}
	if (!ir_alloc_vreg(f, &result_reg))
		return (0);

	IRInstruction call_inst = {
		.opcode = IR_CALL,
		.type = node->value_type,
		.dest = result_reg,
		.func_name = node->call.function_name
	};
	emit(a, f, call_inst);
	return (result_reg);
}

static size_t gen_unary(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	size_t		operand;
	size_t		dest;
	IROpcode	op;

	operand = gen_expression(a, f, node->unary.operand, symbol_table);
	if (operand == 0)
		return (0);
	if (!ir_alloc_vreg(f, &dest))
		return (0);
	op = (node->type == AST_NOT) ? IR_NOT : IR_NEG;
	emit(a, f, (IRInstruction){
			.opcode = op,
			.type = node->value_type,
			.dest = dest, 
			.src_1 = operand 
			});
	return (dest);
}

static size_t gen_binary_op(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	size_t		left = gen_expression(a, f, node->binary.left, symbol_table);
	if (left == 0)
		return (0);
	size_t		right = gen_expression(a, f, node->binary.right, symbol_table);
	if (right == 0)
		return (0);
	size_t		dest;
	if (!ir_alloc_vreg(f, &dest))
		return (0);

	IROpcode	op;
	switch (node->type)
	{
		case AST_ADD:			op = IR_ADD; break;
		case AST_SUB:			op = IR_SUB; break;
		case AST_MUL:			op = IR_MUL; break;
		case AST_DIV:			op = IR_DIV; break;
		case AST_EQUAL:			op = IR_EQ; break;
		case AST_NOT_EQUAL:		op = IR_NEQ; break;
		case AST_LESS:			op = IR_LT; break;
		case AST_LESS_EQUAL:	op = IR_LE; break;
		case AST_GREATER:		op = IR_GT; break;
		case AST_GREATER_EQUAL:	op = IR_GE; break;
		case AST_BIT_AND:
		case AST_BIT_NOT:
		case AST_BIT_OR:
		case AST_BIT_XOR:
		case AST_LSHIFT:
		case AST_RSHIFT:
								fprintf(stderr, 
										"Internal error: " \
										"Bitwise NOT yet implemented\n");
		default:				op = IR_ADD; break;
	}
	IRInstruction inst = {
		.opcode = op,
		.type = node->value_type,
		.dest = dest,
		.src_1 = left,
		.src_2 = right 
	};
	emit(a, f, inst);
	return (dest);
}

static size_t gen_expression(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	if (!node)
		return (0);

	switch (node->type)
	{
		case AST_NUMBER:
			return (gen_number(a, f, node));
		case AST_IDENTIFIER:
			return (gen_identifier(a, f, node, symbol_table));
		case AST_CALL:
			return (gen_call(a, f, node, symbol_table));
		case AST_NEGATE:
		case AST_NOT:
			return (gen_unary(a, f, node, symbol_table));
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_EQUAL:
		case AST_NOT_EQUAL:
		case AST_LESS:
		case AST_LESS_EQUAL:
		case AST_GREATER:
		case AST_GREATER_EQUAL:
			return (gen_binary_op(a, f, node, symbol_table));
		case AST_BIT_AND:
		case AST_BIT_NOT:
		case AST_BIT_OR:
		case AST_BIT_XOR:
		case AST_LSHIFT:
		case AST_RSHIFT:
			error_add(f->errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR,
					f->filename, node->line, node->column,
					"bitwise NOT yet implemented");
			return (0);
		case AST_VAR_DECL:
		case AST_ASSIGNMENT:
		case AST_RETURN:
		case AST_IF:
		case AST_WHILE:
		case AST_BLOCK:
		case AST_FUNCTION:
		case AST_TRANSLATION_UNIT:
			error_add(f->errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR,
					f->filename, node->line, node->column,
					"statement node in expression context");
			return (0);
	}
}

static void gen_if(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	size_t		cond_reg = gen_expression(a, f, node->if_stmt.condition, symbol_table);
	DataType	cond_type = node->if_stmt.condition->value_type;
	if (node->if_stmt.else_branch)
	{
		size_t	label_else = f->label_count++;
		size_t	label_end = f->label_count++;
		if (cond_reg == 0)
			return;

		emit(a, f, (IRInstruction){ 
				.opcode = IR_JZ,
				.type = cond_type, 
				.src_1 = cond_reg,
				.label_id = label_else });
		gen_statement(a, f, node->if_stmt.then_branch, symbol_table, last_reg);
		emit(a, f, (IRInstruction){
				.opcode = IR_JMP,
				.type = TYPE_VOID,
				.label_id = label_end });
		emit(a, f, (IRInstruction){
				.opcode = IR_LABEL,
				.type = TYPE_VOID,
				.label_id = label_else });
		if (node->if_stmt.else_branch)
			gen_statement(a, f, node->if_stmt.else_branch, symbol_table, last_reg);
		emit(a, f, (IRInstruction){ 
				.opcode = IR_LABEL,
				.type = TYPE_VOID,
				.label_id = label_end });
	}
	else
	{
		size_t	label_end = f->label_count++;
		if (cond_reg == 0)
			return;

		emit(a, f, (IRInstruction){ 
				.opcode = IR_JZ, 
				.type = cond_type,
				.src_1 = cond_reg,
				.label_id = label_end });
		gen_statement(a, f, node->if_stmt.then_branch, symbol_table, last_reg);
		emit(a, f, (IRInstruction) {
				.opcode = IR_LABEL,
				.type = TYPE_VOID,
				.label_id = label_end });
	}
}

static void	gen_while(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	size_t		label_start = f->label_count++;
	size_t		label_end = f->label_count++;
	size_t		cond_reg;
	DataType	cond_type = node->while_stmt.condition->value_type;

	emit(a, f, (IRInstruction){ 
			.opcode = IR_LABEL,
			.type = TYPE_VOID,
			.label_id = label_start });
	cond_reg = gen_expression(a, f, node->while_stmt.condition, symbol_table);
	if (cond_reg == 0)
		return;
	emit(a, f, (IRInstruction){ 
			.opcode = IR_JZ,
			.type = cond_type,
			.src_1 = cond_reg,
			.label_id = label_end });
	gen_statement(a, f, node->while_stmt.body, symbol_table, last_reg);
	emit(a, f, (IRInstruction){
			.opcode = IR_JMP,
			.type = TYPE_VOID,
			.label_id = label_start });
	emit(a, f, (IRInstruction){
			.opcode = IR_LABEL,
			.type = TYPE_VOID,
			.label_id = label_end });
}

static void gen_var_decl(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	size_t	stack_idx = f->stack_count++;
	size_t	init_reg;
	
	symbol_table_add(symbol_table, node->var_decl.var_name, stack_idx);
	Symbol *sym = symbol_table_lookup(symbol_table, node->var_decl.var_name);
	if (sym)
		sym->is_stack = true;
	if (node->var_decl.initializer)
	{
		init_reg = gen_expression(a, f, node->var_decl.initializer, symbol_table);
		if (init_reg == 0)
			return;
	}
	else
	{
		if (!ir_alloc_vreg(f, &init_reg))
			return;
		IRInstruction inst = { 
			.opcode = IR_CONST,
			.type = node->var_decl.var_type,
			.dest = init_reg, 
			.imm = 0 };
		emit(a, f, inst);
	}
	IRInstruction store = {
		.opcode = IR_STORE,
		.type = node->var_decl.var_type,
		.dest = stack_idx,
		.src_1 = init_reg
	};
	emit(a, f, store);
	*last_reg = init_reg;
}

static void gen_assignment(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	Symbol		*sym = symbol_table_lookup(symbol_table, node->assignment.var_name);
	IROpcode	opcode = IR_MOV;
	size_t		val_reg;

	if (!sym)
	{
		error_add(f->errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR,
				f->filename, node->line, node->column,
				"assignment to undefined variable '%.*s'",
				(int)node->assignment.var_name.len,
				node->assignment.var_name.start);
		return;
	}

	val_reg = gen_expression(a, f, node->assignment.value, symbol_table);
	if (val_reg == 0)
		return;

	if (sym->is_stack)
		opcode = IR_STORE;
	IRInstruction instruction = {
		.opcode = opcode,
		.type = node->value_type,
		.dest = sym->index,
		.src_1 = val_reg
	};
	emit(a, f, instruction);
	*last_reg = val_reg;
}

static void gen_return(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table)
{
	size_t ret_reg = gen_expression(a, f, node->return_stmt.expression, symbol_table);
	if (ret_reg == 0)
		return;

	emit(a, f, (IRInstruction){ 
			.opcode = IR_RET,
			.type = node->value_type,
			.src_1 = ret_reg });
}

static void gen_block(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	ScopeChange	*watermark = symbol_table->changes;

	for (size_t i = 0; i < node->block.count; ++i)
		gen_statement(a, f, node->block.statements[i], symbol_table, last_reg);
	symbol_table_restore(symbol_table, watermark);
}

static void gen_statement(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symbol_table, size_t *last_reg)
{
	if (!node)
		return;
	switch (node->type)
	{
		case AST_VAR_DECL:
			gen_var_decl(a, f, node, symbol_table, last_reg);
			break;
		case AST_ASSIGNMENT:
			gen_assignment(a, f, node, symbol_table, last_reg);
			break;
		case AST_RETURN:
			gen_return(a, f, node, symbol_table);
			break;
		case AST_BLOCK:
			gen_block(a, f, node, symbol_table, last_reg);
			break;
		case AST_IF:
			gen_if(a, f, node, symbol_table, last_reg);
			break;
		case AST_WHILE:
			gen_while(a, f, node, symbol_table, last_reg);
			break;
		default:
			*last_reg = gen_expression(a, f, node, symbol_table);
			break;
	}
}

IRFunction *ir_gen(Arena *a, ASTNode *root, ErrorContext *errors, const char *filename)
{
	if (!root)
		return (NULL);

	IRFunction *f = arena_alloc(a, sizeof(IRFunction));
	f->vreg_count = 1;
	f->label_count = 0;
	f->total_count = 0;
	f->head = NULL;
	f->tail = NULL;
	f->errors = errors;
	f->filename = filename;

	SymbolTable symbol_table = { .arena = a, .changes = NULL };
	size_t result_reg = 0;

	if (root->type == AST_FUNCTION)
	{
		f->name = root->function.name;
		for (size_t i = 0; i < root->function.param_count; ++i)
		{
			Parameter *param = &root->function.params[i];
			size_t vreg;
			if (!ir_alloc_vreg(f, &vreg))
				return (NULL);
			symbol_table_add(&symbol_table, param->name, vreg);
		}
		ASTNode *body = root->function.body;
		if (body && body->type == AST_BLOCK)
		{
			for (size_t i = 0; i < body->block.count; ++i)
				gen_statement(a, f, body->block.statements[i], &symbol_table, &result_reg);
		}
	}
	else if (root->type == AST_BLOCK)
	{
		for (size_t i = 0; i < root->block.count; ++i)
			gen_statement(a, f, root->block.statements[i], &symbol_table, &result_reg);
	}
	else
	{
		result_reg = gen_expression(a, f, root, &symbol_table);
		if (result_reg == 0)
			return (NULL);
		IRInstruction ret = { 
			.opcode = IR_RET,
			.type = root->value_type,
			.src_1 = result_reg };
		emit(a, f, ret);
	}
	return (f);
}
