#include "jit.h"
#include "jit_internal.h"
#include <stddef.h>
#include <stdint.h>

void	load_location_to_reg(uint8_t **buf, size_t *size, X86Reg dst, Location loc)
{
	switch (loc.type)
	{
		case LOC_REG:
			if (loc.reg != dst)
				emit_mov_reg_reg(buf, size, dst, loc.reg);
			break;
		case LOC_STACK:
			emit_load_param(buf, size, dst, loc.offset);
			break;
		case LOC_CONST:
			emit_mov_imm(buf, size, dst, (uint64_t)loc.imm);	// TODO fix uint64_t
			break;
		case LOC_NONE:
			fprintf(stderr, "Warning: Attempting to load LOC_NONE\n");
			break;
	}
}

void	store_reg_to_location(uint8_t **buf, size_t *size, Location loc, X86Reg src)
{
	switch (loc.type)
	{
		case LOC_REG:
			if (loc.reg != src)
				emit_mov_reg_reg(buf, size, loc.reg, src);
			break;
		case LOC_STACK:
			emit_store_local(buf, size, src, loc.offset);
			break;
		case LOC_CONST:
		case LOC_NONE:
			fprintf(stderr, "Error: Cannot store to LOC_CONST or LOC_NONE\n");
			break;
	}
}

/* -- Load two operands for a binary operation -- */
void	load_binary_operands(uint8_t **buf, size_t *size, X86Reg dst, X86Reg tmp, Location left, Location right, X86Reg *out_right_reg)
{
	load_location_to_reg(buf, size, dst, left);
	if (right.type == LOC_REG)
		*out_right_reg = right.reg;
	else
	{
		load_location_to_reg(buf, size, tmp, right);
		*out_right_reg = tmp;
	}
}

/* -- Common pattern for standard ALU binary operations -- **/
size_t	emit_standard_binary_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx, X86Opcode opcode)
{
	uint8_t		*curr = buf;
	size_t		size = 0;

	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx, inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	X86Reg		right_reg;
	load_binary_operands(&curr, &size, REG_RAX, REG_RDX, src_1, src_2, &right_reg);
	emit_alu(&curr, &size, opcode, REG_RAX, right_reg);
	store_reg_to_location(&curr, &size, dest, REG_RAX);
	return (size);
}

size_t	emit_comparison_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx, X86Condition condition)
{
	uint8_t		*curr = buf;
	size_t		size = 0;

	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx, inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	X86Reg		right_reg;
	load_binary_operands(&curr, &size, REG_RAX, REG_RCX, src_1, src_2, &right_reg);
	emit_cmp(&curr, &size, REG_RAX, right_reg);
	emit_setcc(&curr, &size, condition, REG_RAX);
	emit_movzx(&curr, &size, REG_RAX, REG_RAX);
	store_reg_to_location(&curr, &size, dest, REG_RAX);
	return (size);
}

size_t	emit_shift_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx, X86Extension extension)
{
	uint8_t		*curr = buf;
	size_t		size = 0;
	Location	dest = get_location(ctx, inst->dest);
	Location	src_1 = get_location(ctx, inst->src_1);
	Location	src_2 = get_location(ctx, inst->src_2);

	// Load value to shift into RAX
	load_location_to_reg(&curr, &size, REG_RAX, src_1);
	// Load shift amount into RCX
	load_location_to_reg(&curr, &size, REG_RCX, src_2);
	// Emit SHL/SAR r/m64, CL
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, OP_SHIFT_CL);

	emit_u8(&curr, &size, MOD_REG | (extension << 3) | REG_RAX);

	// Store result
	store_reg_to_location(&curr, &size, dest, REG_RAX);
	return (size);
}
