#include "jit_internal.h"

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

/* =============================== */
/* INDIVIDUAL INSTRUCTION ENCODERS */
/* =============================== */

size_t	encode_const(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	(void)cs;
	(void)cnt;

	emit_mov_imm(&curr, &size, REG_RAX, inst->imm);
	emit_store_local(&curr, &size, REG_RAX, dest_disp);
	return (size);
}

size_t	encode_add(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ALU_ADD));
}

size_t	encode_sub(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
	(void)cnt;
	return (emit_bin_op_std(buf, inst, ALU_SUB));
}

size_t	encode_mul(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
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

size_t	encode_div(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
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

size_t	encode_label(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)inst;
	(void)cs;
	// TODO
	return (0);
}

size_t encode_jmp(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)inst;
	(void)cs;
	// TODO: Implement unconditional jump
	return (0);
}

size_t encode_jz(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)inst;
	(void)cs;
	// TODO: Implement jump if zero
	return (0);
}

size_t encode_jnz(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)inst;
	(void)cs;
	// TODO: Implement jump if not zero
	return (0);
}

size_t encode_neg(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
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

size_t encode_not(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)inst;
	(void)cs;
	// TODO: Implement logical NOT
	return (0);
}

size_t encode_arg(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)buf;
	(void)cnt;
	(void)cs;
	pending_call.arg_vregs[pending_call.count++] = inst->src_1;
	return (0);
}

size_t encode_call(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *call_sites)
{
	(void)cnt;
	uint8_t	*curr = buf;
	size_t	size = 0;
	int32_t	dest_disp = get_slot(inst->dest);
	
	for (size_t i = 0; i < pending_call.count; ++i)
	{
		int32_t arg_disp = get_slot(pending_call.arg_vregs[i]);
		if (i < 6)
			emit_load_param(&curr, &size, arg_registers[i], arg_disp);
		else
		{
			emit_load_param(&curr, &size, REG_RAX, arg_disp);
			emit_u8(&curr, &size, OP_PUSH + REG_RAX);
		}
	}
	size_t stack_args = (pending_call.count > 6) ? (pending_call.count - 6) : 0;
	bool needs_alignment = (stack_args % 2) != 0;
	if (needs_alignment)
	{
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
		emit_u32(&curr, &size, 8);
	}
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, MOV_IMM_R + REG_RAX);
	if (call_sites && buf)
	{
		CallSite site = {
			.patch_location = curr,
			.target_name = inst->func_name
		};
		if (call_sites->count < call_sites->capacity)
			call_sites->sites[call_sites->count++] = site;
	}
	emit_u64(&curr, &size, 0xDEADBEEFDEADBEEF); // Placeholder
	emit_u8(&curr, &size, OP_CALL_IND);
	emit_u8(&curr, &size, MOD_REG | (EXT_CALL << 3) | REG_RAX);
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
	pending_call.count = 0;
	return (size);
}

size_t encode_ret(uint8_t *buf, size_t *cnt, IRInstruction *inst, CallSiteList *cs)
{
	(void)cs;
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

