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
	int32_t	rel = (int32_t)(target - (loc + 4));
	memcpy(loc, &rel, sizeof(int32_t));
}

/* --- Private Helper for Standard ALU Ops --- */
static inline size_t		emit_bin_op_std(uint8_t *buf, IRInstruction *inst,
				JITContext *ctx, X86Opcode opcode)
{
	uint8_t	*curr = buf;
	size_t	size = 0;

	Location dest = get_location(ctx, inst->dest);
	Location src_1 = get_location(ctx, inst->src_1);
	Location src_2 = get_location(ctx, inst->src_2);

	X86Reg working_reg = REG_RAX;
	if (src_1.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, working_reg, src_1.reg);
	else
	 	emit_load_param(&curr, &size, working_reg, src_1.offset);
	if (src_2.type == LOC_REG)
		emit_alu(&curr, &size, opcode, working_reg, src_2.reg);
	else
	{
		emit_load_param(&curr, &size, REG_RDX, src_2.offset);
		emit_alu(&curr, &size, opcode, working_reg, REG_RDX);
	}
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, working_reg);
	else
		emit_store_local(&curr, &size, working_reg, dest.offset);

	return (size);
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
		int32_t rel = (int32_t)(target - (curr + 4));
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
		emit_load_param(buf, size, REG_RAX, src.offset);
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
    if (dest.type == LOC_REG && src.type == LOC_REG)
        emit_mov_reg_reg(&curr, &size, dest.reg, src.reg);
    else if (dest.type == LOC_REG && src.type == LOC_STACK)
        emit_load_param(&curr, &size, dest.reg, src.offset);
    else if (dest.type == LOC_STACK && src.type == LOC_REG)
        emit_store_local(&curr, &size, src.reg, dest.offset);
    else
    {
        emit_load_param(&curr, &size, REG_RAX, src.offset);
        emit_store_local(&curr, &size, REG_RAX, dest.offset);
    }

    return (size);
}

size_t	encode_cmp(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location dest = get_location(ctx, inst->dest);
	Location src_1 = get_location(ctx, inst->src_1);
	Location src_2 = get_location(ctx, inst->src_2);
	X86Condition cc = CC_E;

	if (src_1.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, REG_RAX, src_1.reg);
	else
		emit_load_param(&curr, &size, REG_RAX, src_1.offset);

	X86Reg operand = REG_RCX;
	if (src_2.type == LOC_REG)
		operand = src_2.reg;
	else
		emit_load_param(&curr, &size, REG_RCX, src_2.offset);
	emit_cmp(&curr, &size, REG_RAX, operand);
	switch (inst->opcode)
	{
		case IR_EQ:		cc = CC_E; break;
		case IR_NEQ:	cc = CC_NE; break;
		case IR_LT:		cc = CC_L; break;
		case IR_LE:		cc = CC_LE; break;
		case IR_GT:		cc = CC_G; break;
		case IR_GE:		cc = CC_GE; break;
		default: break;
	}
	emit_setcc(&curr, &size, cc, REG_RAX);
	emit_movzx(&curr, &size, REG_RAX, REG_RAX);
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	return (size);
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
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location	dest = get_location(ctx, inst->dest);

	emit_mov_imm(&curr, &size, REG_RAX, inst->imm);
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	return (size);
}

size_t	encode_add(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ctx, ALU_ADD));
}

size_t	encode_sub(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ctx, ALU_SUB));
}

size_t	encode_mul(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx,inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	if (src_1.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, REG_RAX, src_1.reg);
	else
	 	emit_load_param(&curr, &size, REG_RAX, src_1.offset);

	X86Reg operand = REG_RCX;
	if (src_2.type == LOC_REG)
		operand = src_2.reg;
	else
	 	emit_load_param(&curr, &size, REG_RCX, src_2.offset);
	emit_imul_r64(&curr, &size, REG_RAX, operand);
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	return (size);
}

size_t	encode_div(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx,inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	if (src_1.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, REG_RAX, src_1.reg);
	else
		emit_load_param(&curr, &size, REG_RAX, src_1.offset);

	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_CQO);

	X86Reg divisor = REG_RCX;
	if (src_2.type == LOC_REG)
		divisor = src_2.reg;
	else
		emit_load_param(&curr, &size, REG_RCX, src_2.offset);

	uint8_t rex = REX_W;
	if (divisor >= 8)
		rex |= 0x01;
	emit_u8(&curr, &size, rex);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_IDIV << 3) | (divisor & 7));

	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	return (size);
}

size_t encode_neg(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location dest = get_location(ctx, inst->dest);
	Location src = get_location(ctx, inst->src_1);

	if (src.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, REG_RAX, src.reg);
	else
		emit_load_param(&curr, &size, REG_RAX, src.offset);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_NEG << 3) | REG_RAX);
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	return (size);
}

size_t encode_not(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location dest = get_location(ctx, inst->dest);
	Location src = get_location(ctx, inst->src_1);

	if (src.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, REG_RAX, src.reg);
	else
		emit_load_param(&curr, &size, REG_RAX, src.offset);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, ALU_IMM8);
	emit_u8(&curr, &size, MOD_REG | (EXT_CMP << 3) | REG_RAX);
	emit_u8(&curr, &size, 0x00);
	emit_setcc(&curr, &size, CC_E, REG_RAX);
	emit_movzx(&curr, &size, REG_RAX, REG_RAX);
	if (src.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
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
	PendingCall 	*pc = &ctx->pending_call;
	CallSiteList	*cs = &ctx->call_sites;

	// 1. Calculate stack arguments
	size_t	stack_args = (pc->count > 6) ? (pc->count - 6) : 0;
	size_t	reg_args = (pc->count > 6) ? 6 : pc->count;

	// 2. Apply alignment first before pushing
	bool 	needs_alignment = (stack_args % 2) != 0;
	if (needs_alignment)
	{
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
		emit_u32(&curr, &size, 8);
	}

	// 3. Push stack arguments in reverse order
	for (size_t i = pc->count; i > 6; --i)
	{
		size_t	idx = i - 1;
		Location loc = get_location(ctx, pc->arg_vregs[idx]);
		if (loc.type == LOC_REG)
			emit_mov_reg_reg(&curr, &size, REG_RAX, loc.reg);
		else
			emit_load_param(&curr, &size, REG_RAX, loc.offset);
		emit_push(&curr, &size, REG_RAX);
	}

	// 4. Handle register args
	//    a. Push params to stack
	for (int i = (int)reg_args; i > 0; --i)
	{
		int idx = i - 1;
		Location loc = get_location(ctx, pc->arg_vregs[idx]);
		if (loc.type == LOC_REG)
			emit_push(&curr, &size, loc.reg);
		else
		{
			emit_load_param(&curr, &size, REG_RAX, loc.offset);
			emit_push(&curr, &size, REG_RAX);
		}
	}

	//    b. Pop into abi registers
	for (size_t i = 0; i < reg_args; ++i)
		emit_pop(&curr, &size, arg_registers[i]);
	// 5. Emit call
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, MOV_IMM_R + REG_RAX);
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
		size_t cleanup = stack_args * 8 + (needs_alignment ? 8 : 0);
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_ADD << 3) | REG_RSP);
		emit_u32(&curr, &size, cleanup);
	}
	if (dest.type == LOC_REG)
		emit_mov_reg_reg(&curr, &size, dest.reg, REG_RAX);
	else
		emit_store_local(&curr, &size, REG_RAX, dest.offset);
	pc->count = 0;
	return (size);
}

size_t encode_ret(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	Location src = get_location(ctx, inst->src_1);

	if (src.type == LOC_REG)
	{
		if (src.reg != REG_RAX)
			emit_mov_reg_reg(&curr, &size, REG_RAX, src.reg);
	}
	else
		emit_load_param(&curr, &size, REG_RAX, src.offset);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_LEA);
	emit_u8(&curr, &size, MOD_MEM_DISP8 | (REG_RSP << 3) | REG_RBP);
	emit_u8(&curr, &size, (uint8_t)(-CALLEE_SAVED_SIZE));
	emit_pop(&curr, &size, (X86Reg)15);
    emit_pop(&curr, &size, (X86Reg)14);
    emit_pop(&curr, &size, (X86Reg)13);
    emit_pop(&curr, &size, (X86Reg)12);
    emit_pop(&curr, &size, REG_RBX);

    emit_pop(&curr, &size, REG_RBP);
    emit_u8(&curr, &size, OP_RET);
	return (size);
}

