#include "jit.h"
#include "defines.h"
#include "jit_internal.h"
#include "ir.h"
#include "layout.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

// Calling convention
const X86Reg arg_registers[6] = {
	REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9,
};

// Shared helper
int32_t get_slot(size_t vreg)
{
	return -((int32_t)(vreg + 1) * 8);
}

/* ============== */
/* DISPATCH TABLE */
/* ============== */

typedef size_t (*InstructionEncoder)(uint8_t*, size_t*, IRInstruction*, JITContext*);

static const InstructionEncoder encoders[] =
{
	#define X_OP(opcode, name, fmt, encoder) encoder,
	#include "ir_ops.def"
	#undef X_OP
};

static size_t	encode_inst(uint8_t *buf, IRInstruction *inst, JITContext *ctx)
{
	return (encoders[inst->opcode](buf, NULL, inst, ctx));
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
	ctx->registry.capacity = MAX_FUNCTION_COUNT;
	ctx->registry.count = 0;
	ctx->registry.functions = arena_alloc(a, sizeof(CompiledFunction) * ctx->registry.capacity);

	ctx->call_sites.capacity = MAX_CALL_SITES;
	ctx->call_sites.count = 0;
	ctx->call_sites.sites = arena_alloc(a, sizeof(CallSite) * ctx->call_sites.capacity);
	memset(&ctx->pending_call, 0, sizeof(PendingCall));
}

JITResult jit_compile_function(JITContext *ctx, IRFunction *ir_func, ASTNode *func)
{
	StringView func_name = func->function.name;
	size_t param_count = func->function.param_count;

	JITResult result = {0};
	ctx->pending_call.count = 0;
	memset(ctx->label_offset, 0, sizeof(ctx->label_offset));
	memset(ctx->label_defined, 0, sizeof(ctx->label_defined));
	ctx->patches = NULL;

	if (ir_func->label_count >= MAX_LABELS)
	{
		fprintf(stderr, BOLD_RED "  > too many labels in function '%.*s' (max %d)\n" RESET,
				(int)func_name.len, func_name.start, MAX_LABELS);
		return (result);
	}

    size_t stack_bytes = (ir_func->vreg_count * 8 + 15) & ~15;

	// == PASS 1: Calculate size ===
    size_t predicted_size = encode_prologue(NULL, stack_bytes, param_count);
    IRChunk *chunk = ir_func->head;
	size_t instruction_count = 0;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
		{
            predicted_size += encode_inst(NULL, &chunk->instructions[i], ctx);
			instruction_count++;
		}
        chunk = chunk->next;
    }

	assert(instruction_count == ir_func->total_count && "IR instruction count mismatch");
	
	// === Allocate ===
	result.code = arena_alloc_aligned(ctx->arena, predicted_size, 16);
    if (!result.code)
	{
		fprintf(stderr, BOLD_RED "  > failed to allocate %zu bytes for JIT code\n" RESET,
				predicted_size);
	}

	// === PASS 2: Emit ===
	uint8_t *write_ptr = result.code;
	size_t prologue_size = encode_prologue(write_ptr, stack_bytes, param_count);
	write_ptr += prologue_size;
    
    chunk = ir_func->head;
    while (chunk)
	{
        for (size_t i = 0; i < chunk->count; ++i)
		{
			size_t inst_size = encode_inst(write_ptr, &chunk->instructions[i], ctx);
            write_ptr += inst_size;
			size_t written = write_ptr - result.code;
			if (written > predicted_size)
			{
				fprintf(stderr, BOLD_RED
						"  > JIT CODE GENERATION BUG: buffer overrun!\n"
						"  	 Predicted: %zu bytes\n"
						"    Actually wrote: %zu bytes\n"
						"    Instruction: %s\n" RESET,
						predicted_size, written,
						ir_opcode_name(chunk->instructions[i].opcode));
				abort();
			}
		}
        chunk = chunk->next;	
    }

	// === Verify size match ===
	size_t actual_size = write_ptr - result.code;
	if (actual_size != predicted_size)
	{
		fprintf(stderr, BOLD_RED
				"  > JIT CODE GENERATION BUG: size mismatch!\n"
				"    Pass 1 predicted: %zu bytes\n"
				"    Pass 2 generated: %zu bytes\n"
				"    Difference: %zd bytes\n" RESET,
				predicted_size, actual_size,
				(ssize_t)actual_size - (ssize_t)predicted_size);
		fprintf(stderr,"\n  > IR dump for failed function:\n");
		ir_print(ir_func);
		abort();
	}

	result.size = actual_size;

	// === Register this function ===
    if (ctx->registry.count >= MAX_FUNCTION_COUNT)
	{
		fprintf(stderr, BOLD_RED
				"  > too many functions (max %d)\n" RESET,
				MAX_FUNCTION_COUNT);
		return ((JITResult){0});
	}
	ctx->registry.functions[ctx->registry.count++] = (CompiledFunction){
            .name = func_name,
            .code_addr = result.code,
            .code_size = result.size
    };
	return (result);
}

bool jit_link_all(JITContext *ctx, ErrorContext *errors)
{
	bool	success = true;

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
			error_add(errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR, NULL, 0, 0,
					"undefined reference to function '%.*s'",
					(int)site->target_name.len, site->target_name.start);
			success = false;
			continue;
		}
		
		uint64_t addr = (uint64_t)target_addr;
		memcpy(site->patch_location, &addr, 8);
	}
	return (success);
}
