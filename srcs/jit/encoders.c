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
static inline size_t	emit_bin_op_std(uint8_t *buf, IRInstruction *inst, int x86_opcode)
{
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_1_disp = get_slot(inst->src_1);
	int32_t	src_2_disp = get_slot(inst->src_2);

	emit_load_param(&curr, &size, REG_RAX, src_1_disp);
	emit_load_param(&curr, &size, REG_RCX, src_2_disp);
	emit_alu(&curr, &size, x86_opcode, REG_RAX, REG_RCX);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
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

static inline void	emit_jz_jnz(uint8_t **buf, size_t *size, IRInstruction *inst)
{
	int32_t	slot = get_slot(inst->src_1);
	emit_load_param(buf, size, REG_RAX, slot);
	emit_u8(buf, size, REX_W);
	emit_u8(buf, size, ALU_IMM8);
	emit_u8(buf, size, MOD_REG | (EXT_CMP << 3) | REG_RAX);
	emit_u8(buf, size, 0x00);
}

/* =============================== */
/* INDIVIDUAL INSTRUCTION ENCODERS */
/* =============================== */

size_t encode_mov(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
    (void)ctx;
    (void)cnt;
    uint8_t	*curr = buf;
    size_t	size = 0;
    int32_t	dest_disp = get_slot(inst->dest);
    int32_t	src_disp  = get_slot(inst->src_1);

    emit_load_param(&curr, &size, REG_RAX, src_disp);
    emit_store_local(&curr, &size, REG_RAX, dest_disp);
    return (size);
}

size_t	encode_cmp(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_1_disp = get_slot(inst->src_1);
	int32_t	src_2_disp = get_slot(inst->src_2);
	X86Condition cc = CC_E;

	emit_load_param(&curr, &size, REG_RAX, src_1_disp);
	emit_load_param(&curr, &size, REG_RCX, src_2_disp);
	emit_cmp(&curr, &size, REG_RAX, REG_RCX);
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
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
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

	emit_jz_jnz(&curr, &size, inst);
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
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	(void)ctx;
	(void)cnt;

	emit_mov_imm(&curr, &size, REG_RAX, inst->imm);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	return (size);
}

size_t	encode_add(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ALU_ADD));
}

size_t	encode_sub(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ALU_SUB));
}

size_t	encode_mul(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_1_disp = get_slot(inst->src_1);
	int32_t	src_2_disp = get_slot(inst->src_2);

	emit_load_param(&curr, &size, REG_RAX, src_1_disp);
	emit_load_param(&curr, &size, REG_RCX, src_2_disp);
	emit_imul_r64(&curr, &size, REG_RAX, REG_RCX);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	return (size);
}

size_t	encode_div(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_1_disp = get_slot(inst->src_1);
	int32_t	src_2_disp = get_slot(inst->src_2);
	
	emit_load_param(&curr, &size, REG_RAX, src_1_disp);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_CQO);
	emit_load_param(&curr, &size, REG_RCX, src_2_disp);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_IDIV << 3) | REG_RCX);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	return (size);
}

size_t encode_neg(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_disp = get_slot(inst->src_1);
	
	emit_load_param(&curr, &size, REG_RAX, src_disp);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_IDIV);
	emit_u8(&curr, &size, MOD_REG | (EXT_NEG << 3) | REG_RAX);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	return (size);
}

size_t encode_not(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)cnt;
	(void)ctx;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	int32_t	src_disp = get_slot(inst->src_1);

	emit_load_param(&curr, &size, REG_RAX, src_disp);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, ALU_IMM8);
	emit_u8(&curr, &size, MOD_REG | (EXT_CMP << 3) | REG_RAX);
	emit_u8(&curr, &size, 0x00);
	emit_setcc(&curr, &size, CC_E, REG_RAX);
	emit_movzx(&curr, &size, REG_RAX, REG_RAX);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
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
	int32_t			dest_disp = get_slot(inst->dest);
	PendingCall 	*pc = &ctx->pending_call;
	CallSiteList	*cs = &ctx->call_sites;
	
	// 1. Calculate stack arguments
	size_t	stack_args = (pc->count > 6) ? (pc->count - 6) : 0;

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
		int32_t	arg_disp = get_slot(pc->arg_vregs[idx]);
		emit_load_param(&curr, &size, REG_RAX, arg_disp);
		emit_u8(&curr, &size, OP_PUSH + REG_RAX);
	}

	// 4. Load register aguments forward
	for (size_t i = 0; i < pc->count && i < 6; ++i)
	{
		int32_t	arg_disp = get_slot(pc->arg_vregs[i]);
		emit_load_param(&curr, &size, arg_registers[i], arg_disp);
	}

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
		size_t cleanup = stack_args * 8;
		if (needs_alignment)
			cleanup += 8;
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_ADD << 3) | REG_RSP);
		emit_u32(&curr, &size, cleanup);
	}
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	pc->count = 0;
	return (size);
}

size_t encode_ret(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx)
{
	(void)ctx;
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	src_disp = get_slot(inst->src_1);
	
	emit_load_param(&curr, &size, REG_RAX, src_disp);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, MOV_RM_R);
	emit_u8(&curr, &size, MOD_REG | (REG_RBP << 3) | REG_RSP);
	emit_u8(&curr, &size, OP_POP + REG_RBP);
	emit_u8(&curr, &size, OP_RET);
	return (size);
}

