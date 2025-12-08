#include "jit.h"

static void emit_u8(uint8_t **buf, size_t *count, uint8_t byte)
{
	if (buf && *buf)
	{
		**buf = byte;
		(*buf)++;
	}
	(*count)++;
}

static void emit_u32(uint8_t **buf, size_t *count, uint32_t val)
{
	emit_u8(buf, count, val & 0xFF);
	emit_u8(buf, count, (val >> 8) & 0xFF);
	emit_u8(buf, count, (val >> 16) & 0xFF);
	emit_u8(buf, count, (val >> 24) & 0xFF);
}

static void emit_u64(uint8_t **buf, size_t *count, uint64_t val)
{
	emit_u32(buf, count, val & 0xFFFFFFFF);
	emit_u32(buf, count, (val >> 32) & 0xFFFFFFFF);
}

static size_t encode_prologue(uint8_t *buf, size_t stack_size)
{
	uint8_t *curr = buf;
	size_t size = 0;

	emit_u8(&curr, &size, 0x55);
	emit_u8(&curr, &size, 0x48);
	emit_u8(&curr, &size, 0x89);
	emit_u8(&curr, &size, 0xE5);

	if (stack_size > 0)
	{
		emit_u8(&curr, &size, 0x48);
        emit_u8(&curr, &size, 0x81);
        emit_u8(&curr, &size, 0xEC);
        emit_u32(&curr, &size, (uint32_t)stack_size);
	}

	return (size);
}

static int32_t get_slot(size_t vreg)
{
	return -((int32_t)(vreg + 1) * 8);
}

static size_t encode_inst(uint8_t *buf, IRInstruction *inst)
{
	uint8_t *curr = buf;
	size_t size = 0;

	int32_t dest_disp = get_slot(inst->dest);
	int32_t src_1_disp = get_slot(inst->src_1);
	int32_t src_2_disp = get_slot(inst->src_2);
	switch (inst->opcode)
	{
	case IR_CONST:
            // mov rax, imm64
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0xB8);
            emit_u64(&curr, &size, inst->imm);

            // mov [rbp + dest], rax
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x89);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, dest_disp);
            break;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
            // mov rax, [rbp + src1]
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x8B);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, src_1_disp);

            // mov rcx, [rbp + src2]
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x8B);
            emit_u8(&curr, &size, 0x8D);
            emit_u32(&curr, &size, src_2_disp);

            if (inst->opcode == IR_ADD) {
                // add rax, rcx
                emit_u8(&curr, &size, 0x48);
                emit_u8(&curr, &size, 0x01);
                emit_u8(&curr, &size, 0xC8);
            } else if (inst->opcode == IR_SUB) {
                // sub rax, rcx
                emit_u8(&curr, &size, 0x48);
                emit_u8(&curr, &size, 0x29);
                emit_u8(&curr, &size, 0xC8);
            } else if (inst->opcode == IR_MUL) {
                // imul rax, rcx
                emit_u8(&curr, &size, 0x48);
                emit_u8(&curr, &size, 0x0F);
                emit_u8(&curr, &size, 0xAF);
                emit_u8(&curr, &size, 0xC1);
            }

            // mov [rbp + dest], rax
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x89);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, dest_disp);
            break;

		case IR_DIV:
			// mov rax, [rbp + src1] (Dividend)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x8B);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, src_1_disp);

            // cqo (Sign extend RAX into RDX:RAX)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x99);

            // mov rcx, [rbp + src2] (Divisor)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x8B);
            emit_u8(&curr, &size, 0x8D);
            emit_u32(&curr, &size, src_2_disp);

            // idiv rcx (Signed divide RDX:RAX by RCX)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0xF7);
            emit_u8(&curr, &size, 0xF9);

            // mov [rbp + dest], rax (Quotient)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x89);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, dest_disp);
            break;

        case IR_RET:
            // mov rax, [rbp + src1] (Return value)
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x8B);
            emit_u8(&curr, &size, 0x85);
            emit_u32(&curr, &size, src_1_disp);

            // Epilogue: mov rsp, rbp; pop rbp; ret
            emit_u8(&curr, &size, 0x48);
            emit_u8(&curr, &size, 0x89);
            emit_u8(&curr, &size, 0xEC);
            emit_u8(&curr, &size, 0x5D);
            emit_u8(&curr, &size, 0xC3);
            break;
            
        default: break;
	}
	return (size);
}

JITResult jit_compile(Arena *a, IRFunction *func)
{
    JITResult result = {0};

    size_t stack_bytes = (func->vreg_count * 8 + 15) & ~15;

	// Pass 1: calculate size
    size_t total_size = encode_prologue(NULL, stack_bytes);
    IRChunk *chunk = func->head;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
            total_size += encode_inst(NULL, &chunk->instructions[i]);
        chunk = chunk->next;
    }

	// Allocate
	result.code = arena_alloc_aligned(a, total_size, 16);
    if (!result.code) return (result);
    result.size = total_size;

	// Pass 2: emit 
    uint8_t *curr = result.code;
    curr += encode_prologue(curr, stack_bytes);
    
    chunk = func->head;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
            curr += encode_inst(curr, &chunk->instructions[i]);
        chunk = chunk->next;
    }

    return (result);
}
