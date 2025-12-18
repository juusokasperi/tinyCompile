#include "jit_internal.h"
#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ir.h"
#include "layout.h"

// Calling convention
const X86Reg arg_registers[6] = {
	REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9,
};

PendingCall pending_call = {0};

// Shared helper
int32_t get_slot(size_t vreg)
{
	return -((int32_t)(vreg + 1) * 8);
}

/* ============== */
/* DISPATCH TABLE */
/* ============== */

typedef size_t (*InstructionEncoder)(uint8_t*, size_t*, IRInstruction*, CallSiteList*);

static const InstructionEncoder encoders[] =
{
	#define X_OP(opcode, name, fmt, encoder) encoder,
	#include "ir_ops.def"
	#undef X_OP
};

static size_t	encode_inst(uint8_t *buf, IRInstruction *inst, CallSiteList *call_sites)
{
	return (encoders[inst->opcode](buf, NULL, inst, call_sites));
}

/* ============== */
/* MAIN JIT LOGIC */
/* ============== */

size_t encode_prologue(uint8_t *buf, size_t stack_size, size_t param_count)
{
	uint8_t	*curr = buf;
	size_t	size = 0;

	emit_u8(&curr, &size, OP_PUSH + REG_RBP);
	emit_u8(&curr, &size, REX_W);
	emit_u8(&curr, &size, MOV_RM_R);
	emit_u8(&curr, &size, MOD_REG | (REG_RSP << 3) | REG_RBP);
	if (stack_size > 0)
	{
		emit_u8(&curr, &size, REX_W);
        emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
        emit_u32(&curr, &size, (uint32_t)stack_size);
	}
	for (size_t i = 0; i < param_count && i < 6; i++)
	{
        int32_t slot = get_slot(i);
        emit_store_local(&curr, &size, arg_registers[i], slot);
    }
	return (size);
}
void jit_ctx_init(JITContext *ctx, Arena *a)
{
	ctx->arena = a;
	ctx->registry.capacity = MAX_FUNCS;
	ctx->registry.count = 0;
	ctx->registry.functions = arena_alloc(a, sizeof(CompiledFunction) * ctx->registry.capacity);

	ctx->call_sites.capacity = MAX_CALL_SITES;
	ctx->call_sites.count = 0;
	ctx->call_sites.sites = arena_alloc(a, sizeof(CallSite) * ctx->call_sites.capacity);
}

JITResult jit_compile_function(JITContext *ctx, IRFunction *ir_func, ASTNode *func)
{
	StringView func_name = func->function.name;
	size_t param_count = func->function.param_count;

	JITResult result = {0};
	pending_call.count = 0;
    size_t stack_bytes = (ir_func->vreg_count * 8 + 15) & ~15;

	// Pass 1: calculate size
    size_t total_size = encode_prologue(NULL, stack_bytes, param_count);
    IRChunk *chunk = ir_func->head;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
            total_size += encode_inst(NULL, &chunk->instructions[i], NULL);
        chunk = chunk->next;
    }
	
	// Allocate
	result.code = arena_alloc_aligned(ctx->arena, total_size, 16);
    if (!result.code) return (result);
    result.size = total_size;

	// Pass 2: emit 
    uint8_t *curr = result.code;
    curr += encode_prologue(curr, stack_bytes, param_count);
    
    chunk = ir_func->head;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
            curr += encode_inst(curr, &chunk->instructions[i], &ctx->call_sites);
        chunk = chunk->next;	
    }

	// Register this function
    if (ctx->registry.count < ctx->registry.capacity)
	{
        ctx->registry.functions[ctx->registry.count++] = (CompiledFunction){
            .name = func_name,
            .code_addr = result.code,
            .code_size = result.size
        };
    }
	return (result);
}

bool jit_link_all(JITContext *ctx)
{
	for (size_t i = 0; i < ctx->call_sites.count; ++i)
	{
		CallSite *site = &ctx->call_sites.sites[i];

		uint8_t *target_addr = NULL;
		for (size_t j = 0; j < ctx->registry.count; ++j)
		{
			if (sv_eq(site->target_name, ctx->registry.functions[j].name))
			{
				target_addr = ctx->registry.functions[j].code_addr;
				break;
			}
		}

		if (!target_addr)
		{
			fprintf(stderr, BOLD_RED "  > linker error: undefined reference to '%.*s'\n" RESET,
					(int)site->target_name.len, site->target_name.start);
			return (false);
		}
		
		uint64_t addr = (uint64_t)target_addr;
		memcpy(site->patch_location, &addr, 8);
	}
	return (true);
}
