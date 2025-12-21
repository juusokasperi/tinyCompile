#include "jit.h"
#include <stddef.h>
#include <stdint.h>

static uint8_t get_rex(X86Reg dst, X86Reg src)
{
	uint8_t	rex = REX_W;
	if (src >= 8)
		rex |= 0x04;
	if (dst >= 8)
		rex |= 0x01;
	return (rex);
}

void emit_u8(uint8_t **buf, size_t *count, uint8_t byte)
{
	if (buf && *buf)
	{
		**buf = byte;
		(*buf)++;
	}
	(*count)++;
}

void emit_u32(uint8_t **buf, size_t *count, uint32_t val)
{
	emit_u8(buf, count, val & 0xFF);
	emit_u8(buf, count, (val >> 8) & 0xFF);
	emit_u8(buf, count, (val >> 16) & 0xFF);
	emit_u8(buf, count, (val >> 24) & 0xFF);
}

void emit_u64(uint8_t **buf, size_t *count, uint64_t val)
{
	emit_u32(buf, count, val & 0xFFFFFFFF);
	emit_u32(buf, count, (val >> 32) & 0xFFFFFFFF);
}

// 1. Move Register -> Memory (Store)
// "mov [rbp + disp], reg"
void emit_store_local(uint8_t **buf, size_t *cnt, X86Reg src, int32_t disp)
{
	uint8_t	rex = get_rex(0, src);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, MOV_RM_R); // 0x89
    // ModR/M: Mode=DISP32 (0x80) | Reg=src | RM=RBP (5)
    emit_u8(buf, cnt, MOD_MEM_DISP32 | ((src & 7) << 3) | REG_RBP);
    emit_u32(buf, cnt, disp);
}

// 2. Move Memory -> Register (Load)
// "mov reg, [rbp + disp]"
void emit_load_param(uint8_t **buf, size_t *cnt, X86Reg dst, int32_t disp)
{
	uint8_t	rex = get_rex(0, dst);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, MOV_R_RM); // 0x8B
    // ModR/M: Mode=DISP32 (0x80) | Reg=dst | RM=RBP (5)
    emit_u8(buf, cnt, MOD_MEM_DISP32 | ((dst & 7) << 3) | REG_RBP);
    emit_u32(buf, cnt, disp);
}

// 3. Move Immediate -> Register
// "mov reg, 12345"
void emit_mov_imm(uint8_t **buf, size_t *cnt, X86Reg dst, uint64_t imm)
{
	uint8_t	rex = get_rex(dst, 0);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, MOV_IMM_R + (dst & 7)); // 0xB8 + reg
    emit_u64(buf, cnt, imm);
}

// 4. Standard ALU (Add, Sub, Xor, etc.)
// "add dst, src"
void emit_alu(uint8_t **buf, size_t *cnt, X86Opcode op, X86Reg dst, X86Reg src)
{
	uint8_t	rex = get_rex(dst, src);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, op); // e.g., ALU_ADD (0x01)
    // ModR/M: Mode=REG (0xC0) | Reg=src | RM=dst
    emit_u8(buf, cnt, MOD_REG | ((src & 7) << 3) | (dst & 7));
}

void emit_imul_r64(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src)
{
	uint8_t	rex = get_rex(src, dst);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, OP_IMUL_1);
    emit_u8(buf, cnt, OP_IMUL_2);
    emit_u8(buf, cnt, MOD_REG | ((dst & 7) << 3) | (src & 7));
}

// Move register to register
// "mov dst, src"
void emit_mov_reg_reg(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src)
{
    uint8_t	rex = get_rex(dst, src);

    emit_u8(buf, cnt, rex);
    emit_u8(buf, cnt, MOV_RM_R);
    emit_u8(buf, cnt, MOD_REG | ((src & 7) << 3) | (dst & 7));
}

void emit_cmp(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src)
{
	uint8_t	rex = get_rex(dst, src);

	emit_u8(buf, cnt, rex);
	emit_u8(buf, cnt, ALU_CMP);
	emit_u8(buf, cnt, MOD_REG | ((src & 7) << 3) | (dst & 7));
}

void emit_setcc(uint8_t **buf, size_t *cnt, X86Condition cc, X86Reg dst)
{
	uint8_t	rex;

	if (dst >= 8 || (dst >= 4 && dst <= 7))
	{
		rex = 0x40;
		if (dst >= 8)
			rex |= 0x01;
		emit_u8(buf, cnt, rex);
	}
	emit_u8(buf, cnt, OP_PREFIX_0F);
	emit_u8(buf, cnt, 0x90 | cc);
	emit_u8(buf, cnt, MOD_REG | (0 << 3) | (dst & 7));
}

void emit_movzx(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src)
{
	uint8_t	rex = REX_W;

	if (dst >= 8)
		rex |= 0x04;
	if (src >= 8)
		rex |= 0x01;
	emit_u8(buf, cnt, rex);
	emit_u8(buf, cnt, OP_PREFIX_0F);
	emit_u8(buf, cnt, OP_MOVZX);
	emit_u8(buf, cnt, MOD_REG | ((dst & 7) << 3) | (src & 7));
}
