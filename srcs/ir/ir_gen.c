#include "ir.h"
#include <stdlib.h>

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

static size_t gen_expression(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symtab)
{
	if (!node)
		return (0);

	if (node->type == AST_CALL)
	{
		for (size_t i = 0; i < node->call.arg_count; ++i)
		{
			size_t arg_reg = gen_expression(a, f, node->call.args[i], symtab);
			IRInstruction arg_inst = {
				.opcode = IR_ARG,
				.src_1 = arg_reg,
				.imm = i
			};
			emit(a, f, arg_inst);
		}

		size_t result_reg = f->vreg_count++;
		IRInstruction call_inst = {
			.opcode = IR_CALL,
			.dest = result_reg,
			.func_name = node->call.function_name
		};
		emit(a, f, call_inst);
		return (result_reg);
	}
	if (node->type == AST_IDENTIFIER)
	{
        Symbol *sym = symtab_lookup(symtab, node->identifier.name);
		return (sym ? sym->vreg : 0);
    }
    
	if (node->type == AST_NUMBER)
	{
		size_t reg = f->vreg_count++;
		int64_t val = sv_to_int(a, node->number.value);
		IRInstruction inst = {
			.opcode = IR_CONST,
			.dest = reg,
			.imm = val
		};
		emit(a, f, inst);
		return (reg);
	}

	if (node->type >= AST_ADD && node->type <= AST_DIV)
	{
		size_t left = gen_expression(a, f, node->binary.left, symtab);
		size_t right = gen_expression(a, f, node->binary.right, symtab);
		size_t dest = f->vreg_count++;
		IRInstruction inst = { .dest = dest, .src_1 = left, .src_2 = right };

		switch (node->type)
		{
			case AST_ADD: inst.opcode = IR_ADD; break;
			case AST_SUB: inst.opcode = IR_SUB; break;
			case AST_MUL: inst.opcode = IR_MUL; break;
			case AST_DIV: inst.opcode = IR_DIV; break;
			default: break;
		}
		emit(a, f, inst);
		return (dest);
	}

	if (node->type == AST_NEGATE)
	{
		size_t operand = gen_expression(a, f, node->unary.operand, symtab);
		size_t dest = f->vreg_count++;
		IRInstruction inst = { .opcode = IR_NEG, .dest = dest, .src_1 = operand };
		emit(a, f, inst);
		return (dest);
	}
	return (0);
}

static void gen_statement(Arena *a, IRFunction *f, ASTNode *node, SymbolTable *symtab, size_t *last_reg)
{
	if (!node)
		return;
	if (node->type == AST_VAR_DECL)
	{
		// int x = <expr>;
		size_t init_reg;
		if (node->var_decl.initializer)
			init_reg = gen_expression(a, f, node->var_decl.initializer, symtab);
		else
		{
			init_reg = f->vreg_count++;
			IRInstruction inst = { .opcode = IR_CONST, .dest = init_reg, .imm = 0 };
			emit(a, f, inst);
		}
		symtab_add(symtab, node->var_decl.var_name, init_reg);
		*last_reg = init_reg;
		return;
	}
	if (node->type == AST_ASSIGNMENT)
	{
		Symbol *sym = symtab_lookup(symtab, node->assignment.var_name);
		if (!sym)
		{
			fprintf(stderr, "Error: Assignment to undefined variable");
			return;
		}
		size_t val_reg = gen_expression(a, f, node->assignment.value, symtab);
		sym->vreg = val_reg;
		*last_reg = val_reg;
		return;
	}
	if (node->type == AST_RETURN)
	{
		size_t ret_reg = gen_expression(a, f, node->return_stmt.expression, symtab);
		IRInstruction ret = { .opcode = IR_RET, .src_1 = ret_reg };
		emit(a, f, ret);
		return;
	}

	*last_reg = gen_expression(a, f, node, symtab);
}

IRFunction *ir_gen(Arena *a, ASTNode *root)
{
	if (!root)
		return (NULL);

	IRFunction *f = arena_alloc(a, sizeof(IRFunction));
	f->vreg_count = 0;
	f->total_count = 0;
	f->head = NULL;
	f->tail = NULL;

	SymbolTable symtab = {0};
	size_t result_reg = 0;

	if (root->type == AST_FUNCTION)
	{
		f->name = root->function.name;
		for (size_t i = 0; i < root->function.param_count; i++)
		{
            Parameter *param = &root->function.params[i];
            size_t vreg = f->vreg_count++;
            symtab_add(&symtab, param->name, vreg);
        }

		ASTNode *body = root->function.body;
		if (body && body->type == AST_BLOCK)
		{
			for (size_t i = 0; i < body->block.count; ++i)
				gen_statement(a, f, body->block.statements[i], &symtab, &result_reg);
		}
	}
	else if (root->type == AST_BLOCK)
	{
		for (size_t i = 0; i < root->block.count; ++i)
			gen_statement(a, f, root->block.statements[i], &symtab, &result_reg);
	}
	else
	{
		result_reg = gen_expression(a, f, root, &symtab);
		IRInstruction ret = { .opcode = IR_RET, .src_1 = result_reg };
		emit(a, f, ret);
	}

	return (f);
}
