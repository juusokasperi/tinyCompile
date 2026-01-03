#include "defines.h"
#include "ir.h"
#include "jit.h"
#include "jit_internal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void	patch_jump_offset(uint8_t *loc, uint8_t *target)
{
	int32_t	rel = (int32_t)(target - (loc + sizeof(int32_t)));
	memcpy(loc, &rel, sizeof(int32_t));
}

/* --- Locals are below RBP, callee-saved registers and 8-byte alignment pad -- */
static inline int32_t	get_local_offset(size_t idx)
{
	return (-(CALLEE_SAVED_SIZE + WORD_SIZE + (int32_t)(idx + 1) * WORD_SIZE));
}

static inline size_t emit_jump(uint8_t *buf, size_t label_id, JITContext *ctx, uint8_t opcode)
{
	uint8_t	*curr = buf;
	size_t	size = 0;
	if (opcode == OP_JMP_REL32)
		emit_u8(&curr, &size, OP_JMP_REL32);
	else
	{
		emit_u8(&curr, &size, OP_PREFIX_0F);
		emit_u8(&curr, &size, opcode);
	}

	if (!buf)
		return (size + 4);
	if (label_id < MAX_LABELS && ctx->label_defined[label_id])
	{
		uint8_t	*target = ctx->label_offset[label_id];
		int32_t rel = (int32_t)(target - (curr + sizeof(int32_t)));
		emit_u32(&curr, &size, (uint32_t)rel);
	}
	else
	{
		Patch *p = arena_alloc(ctx->data_arena, sizeof(Patch));
		p->label_id = label_id;
		p->loc = curr;
		p->next = ctx->patches;
		ctx->patches = p;
		emit_u32(&curr, &size, 0);
		return (size);
	}

	return (size);
}

static inline void	emit_jz_jnz(uint8_t **buf, size_t *size, JITContext *ctx, IRInstruction *inst)
{
	Location src = get_location(ctx, inst->src_1);
	if (src.type == LOC_REG)
		emit_test(buf, size, src.reg, src.reg);
	else
	{
		load_location_to_reg(buf, size, REG_RAX, src);
		emit_u8(buf, size, REX_W);
		emit_u8(buf, size, ALU_IMM8);
		emit_u8(buf, size, MOD_REG | (EXT_CMP << 3) | REG_RAX);
		emit_u8(buf, size, 0x00);
	}
}

/* =============================== */
/* INDIVIDUAL INSTRUCTION ENCODERS */
/* =============================== */

size_t encode_mov(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location dest = get_location(ctx, inst->dest);
	Location src = get_location(ctx, inst->src_1);

	if (dest.type == LOC_REG && src.type == LOC_REG && dest.reg == src.reg)
		return (0);
	load_location_to_reg(&curr, &size, REG_RAX, src);
	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t	encode_cmp(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	X86Condition cc;

	switch (inst->opcode)
	{
		case IR_EQ:		cc = CC_E;	break;
		case IR_NEQ:	cc = CC_NE;	break;
		case IR_LT:		cc = CC_L;	break;
		case IR_LE:		cc = CC_LE;	break;
		case IR_GT:		cc = CC_G;	break;
		case IR_GE:		cc = CC_GE;	break;
		default:		cc = CC_E;	break;
	}

	return emit_comparison_op(buf, inst, ctx, cc);
}

size_t	encode_jmp(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_jump(buf, inst->label_id, ctx, OP_JMP_REL32));
}

// JZ, JNZ
size_t	encode_branch(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	size_t	jmp_size;
	uint8_t	opcode;

	emit_jz_jnz(&curr, &size, ctx, inst);
	opcode = 0x80 | ((inst->opcode == IR_JZ) ? CC_E : CC_NE);
	jmp_size = emit_jump(curr, inst->label_id, ctx, opcode);
	return (size + jmp_size);
}

size_t encode_label(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	size_t	id = inst->label_id;
	if (id >= MAX_LABELS)
		return (0);
	if (buf)
	{
		ctx->label_defined[id] = true;
		ctx->label_offset[id] = buf;
		Patch *p = ctx->patches;
		Patch *prev = NULL;
		while (p)
		{
			if (p->label_id == id)
			{
				patch_jump_offset(p->loc, buf);
				Patch *next = p->next;
				if (prev)
					prev->next = next;
				else
					ctx->patches = next;
				p = next;
			}
			else
			{
				prev = p;
				p = p->next;
			}
		}
	}
	return (0);
}

size_t	encode_const(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);

	emit_mov_imm(&curr, &size, REG_RAX, inst->imm);
	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t	encode_add(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_standard_binary_op(buf, inst, ctx, ALU_ADD));
}

size_t	encode_sub(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_standard_binary_op(buf, inst, ctx, ALU_SUB));
}

size_t	encode_mul(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx,inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	X86Reg		right_reg;
	load_binary_operands(&curr, &size, REG_RAX, REG_RCX, src_1, src_2, &right_reg);
	
	emit_imul_r64(&curr, &size, REG_RAX, right_reg);
	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t	encode_div(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx, inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	load_location_to_reg(&curr, &size, REG_RAX, src_1);

	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_CQO);

	X86Reg divisor = REG_RCX;
	if (src_2.type == LOC_REG)
		divisor = src_2.reg;
	else
		load_location_to_reg(&curr, &size, REG_RCX, src_2);

	uint8_t rex = REX_W;
	if (divisor >= 8)
		rex |= 0x01;
	emit_u8(&curr, &size, rex);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_IDIV << 3) | (divisor & 7));

	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t encode_neg(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src = get_location(ctx, inst->src_1);

	load_location_to_reg(&curr, &size, REG_RAX, src);

	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_NEG << 3) | REG_RAX);

	store_reg_to_location(&curr, &size, dest, REG_RAX);
	return (size);
}

size_t encode_and(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_standard_binary_op(buf, inst, ctx, ALU_AND));
}

size_t encode_or(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_standard_binary_op(buf, inst, ctx, ALU_OR));
}

size_t encode_xor(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;	
	return (emit_standard_binary_op(buf, inst, ctx, ALU_XOR));
}

size_t encode_not_bitwise(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src = get_location(ctx, inst->src_1);

	// Load operand into RAX
	load_location_to_reg(&curr, &size, REG_RAX, src);
	// Emit NOT r/m64
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_GRP3);
	emit_u8(&curr, &size, MOD_REG | (EXT_NOT << 3) | REG_RAX);

	// Store result
	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t encode_not(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src = get_location(ctx, inst->src_1);

	load_location_to_reg(&curr, &size, REG_RAX, src);

	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, ALU_IMM8);
	emit_u8(&curr, &size, MOD_REG | (EXT_CMP << 3) | REG_RAX);
	emit_u8(&curr, &size, 0x00);

	emit_setcc(&curr, &size, CC_E, REG_RAX);
	emit_movzx(&curr, &size, REG_RAX, REG_RAX);

	store_reg_to_location(&curr, &size, dest, REG_RAX);

	return (size);
}

size_t encode_arg(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)buf;
	(void)cnt;

	PendingCall *pc = &ctx->pending_call;
	pc->arg_vregs[pc->count++] = inst->src_1;

	return (0);
}

size_t encode_call(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t			*curr = buf;
	size_t			size = 0;
	Location		dest = get_location(ctx, inst->dest);
	PendingCall		*pc = &ctx->pending_call;
	CallSiteList	*cs = &ctx->call_sites;

	// 1. Calculate stack arguments
	size_t	stack_args = (pc->count > SYS_V_MAX_REG_ARGS) 
			? (pc->count - SYS_V_MAX_REG_ARGS) : 0;
	size_t	reg_args = (pc->count > SYS_V_MAX_REG_ARGS)
			? SYS_V_MAX_REG_ARGS : pc->count;

	// 2. Apply alignment first before pushing
	bool	needs_alignment = (stack_args % 2) != 0;
	if (needs_alignment)
	{
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
		emit_u32(&curr, &size, STACK_ALIGNMENT / 2);
	}

	// 3. Push stack arguments in reverse order
	for (size_t i = pc->count; i > 6; --i)
	{
		size_t	idx = i - 1;
		Location loc = get_location(ctx, pc->arg_vregs[idx]);
		load_location_to_reg(&curr, &size, REG_RAX, loc);
		emit_push(&curr, &size, REG_RAX);
	}

	// 4. Handle register args
	//	  a. Push params to stack
	for (int i = (int)reg_args - 1; i >= 0; --i)
	{
		Location loc = get_location(ctx, pc->arg_vregs[i]);

		if (loc.type == LOC_REG)
			emit_push(&curr, &size, loc.reg);
		else
		{
			load_location_to_reg(&curr, &size, REG_RAX, loc);
			emit_push(&curr, &size, REG_RAX);
		}
	}

	//	  b. Pop into abi registers
	for (size_t i = 0; i < reg_args; ++i)
		emit_pop(&curr, &size, arg_registers[i]);
	// 5. Emit call
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, MOV_IMM_R + REG_RAX);

	// 6. Record call site for linking
	if (cs && buf)
	{
		CallSite site = {
			.patch_location = curr,
			.target_name = inst->func_name
		};
		if (cs->count < cs->capacity)
			cs->sites[cs->count++] = site;
	}
	emit_u64(&curr, &size, 0xDEADBEEFDEADBEEF); // Placeholder
	emit_u8(&curr, &size, OP_CALL_IND);
	emit_u8(&curr, &size, MOD_REG | (EXT_CALL << 3) | REG_RAX);

	// 6. Cleanup stack (arguments + padding)
	if (stack_args > 0 || needs_alignment)
	{
		size_t cleanup = stack_args * (STACK_ALIGNMENT / 2) 
			+ (needs_alignment ? (STACK_ALIGNMENT / 2) : 0);
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_ADD << 3) | REG_RSP);
		emit_u32(&curr, &size, cleanup);
	}

	store_reg_to_location(&curr, &size, dest, REG_RAX);
	pc->count = 0;
	return (size);
}

size_t encode_ret(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	src = get_location(ctx, inst->src_1);

	// Load return value to RAX
	load_location_to_reg(&curr, &size, REG_RAX, src);

	// Restore stack pointer
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_LEA);
	emit_u8(&curr, &size, MOD_MEM_DISP8 | (REG_RSP << 3) | REG_RBP);
	emit_u8(&curr, &size, (uint8_t)(-CALLEE_SAVED_SIZE));

	// Restore callee-saved registers
	emit_pop(&curr, &size, REG_R15);
	emit_pop(&curr, &size, REG_R14);
	emit_pop(&curr, &size, REG_R13);
	emit_pop(&curr, &size, REG_R12);
	emit_pop(&curr, &size, REG_RBX);

	// Restore base pointer and return
	emit_pop(&curr, &size, REG_RBP);
	emit_u8(&curr, &size, OP_RET);

	return (size);
}

size_t encode_store(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	size_t		stack_idx = inst->dest;
	size_t		src_vreg = inst->src_1;
	Location	src = get_location(ctx, src_vreg);
	int32_t		offset = get_local_offset(stack_idx);

	if (src.type == LOC_REG)
		emit_store_local(&curr, &size, src.reg, offset);
	else
	{
		load_location_to_reg(&curr, &size, REG_RAX, src);
		emit_store_local(&curr, &size, REG_RAX, offset);
	}

	return (size);
}

size_t encode_load(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	size_t		dest_vreg = inst->dest;
	size_t		stack_idx = inst->src_1;
	Location	dest = get_location(ctx, dest_vreg);
	int32_t		offset = get_local_offset(stack_idx);
	
	if (dest.type == LOC_REG)
		emit_load_param(&curr, &size, dest.reg, offset);
	else
	{
		emit_load_param(&curr, &size, REG_RAX, offset);
		store_reg_to_location(&curr, &size, dest, REG_RAX);
	}
	return (size);
}

size_t	encode_lshift(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_shift_op(buf, inst, ctx, EXT_SHL));
}

size_t	encode_rshift(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_shift_op(buf, inst, ctx, EXT_SAR));
}

size_t	encode_urshift(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_shift_op(buf, inst, ctx, EXT_SHR));
}
