#include "jit.h"
#include "compile.h"
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
	if (vreg >= MAX_VREGS_PER_FUNCTION)
	{
		fprintf(stderr, BOLD_RED
				"  > JIT INTERNAL ERROR\n"
				"    vreg %zu exceeds limit %d\n" RESET,
				vreg, MAX_VREGS_PER_FUNCTION);
		abort();
	}
	return -((int32_t)(vreg + 1) * 8);
}

/* ===================== */
/* LINEAR SCAN ALLOCATOR */
/* ===================== */

Location get_location(JITContext *ctx, size_t vreg)
{
	if (ctx->vreg_map[vreg].type != 0)
		return (ctx->vreg_map[vreg]);

	for (int i = 0; i < 16; ++i)
	{
		if (i == REG_RSP || i == REG_RBP
			|| i == REG_RAX || i == REG_RBX)
			continue;
		if (!ctx->phys_regs[i])
		{
			ctx->phys_regs[i] = true;
			ctx->vreg_map[vreg].type = LOC_REG;
			ctx->vreg_map[vreg].reg = (X86Reg)i;
			return (ctx->vreg_map[vreg]);
		}
	}

	// If no space on registers, allocate to stack
	ctx->vreg_map[vreg].type = LOC_STACK;
	ctx->vreg_map[vreg].offset = -((int32_t)(vreg + 1) * 8);
	return (ctx->vreg_map[vreg]);
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

	// 1. Standard prologue (Push RBP, move RBP, sub RSP)
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

	// 2. Handle register arguments
	for (size_t i = 0; i < param_count && i < 6; i++)
	{
        int32_t slot = get_slot(i);
        emit_store_local(&curr, &size, arg_registers[i], slot);
    }

	// 3. Handle stack arguments
	// Copy them from caller's frame to local frame
	for (size_t i = 6; i < param_count; i++)
	{
		// Caller puts 7th arg at [RBP + 16], 8th at [RBP + 24], etc.
		int32_t src_offset = 16 + (i - 6) * 8;
		int32_t dest_slot = get_slot(i);

		// MOV RAX, [RBP + src_offset]  (Load from caller)
		emit_load_param(&curr, &size, REG_RAX, src_offset);

		// MOV [RBP + dest_slot], RAX   (Save to local var)
		emit_store_local(&curr, &size, REG_RAX, dest_slot);
	}
	return (size);
}
void jit_ctx_init(JITContext *ctx, Arena *data_arena, Arena *exec_arena)
{
	ctx->data_arena = data_arena;
	ctx->exec_arena = exec_arena;
	ctx->registry.capacity = MAX_FUNCTION_COUNT;
	ctx->registry.count = 0;
	ctx->registry.functions = arena_alloc(data_arena, sizeof(CompiledFunction) * ctx->registry.capacity);

	ctx->vreg_map = arena_alloc(data_arena, sizeof(Location) * MAX_VREGS_PER_FUNCTION);

	ctx->call_sites.capacity = MAX_CALL_SITES;
	ctx->call_sites.count = 0;
	ctx->call_sites.sites = arena_alloc(data_arena, sizeof(CallSite) * ctx->call_sites.capacity);
	memset(&ctx->pending_call, 0, sizeof(PendingCall));
}


static inline void reset_state(JITContext *ctx)
{
	ctx->pending_call.count = 0;
	ctx->patches = NULL;
	memset(ctx->label_offset, 0, sizeof(ctx->label_offset));
	memset(ctx->label_defined, 0, sizeof(ctx->label_defined));
	memset(ctx->vreg_map, 0, sizeof(Location) * MAX_VREGS_PER_FUNCTION);
	memset(ctx->phys_regs, 0, sizeof(ctx->phys_regs));
}

JITResult jit_compile_function(JITContext *ctx, IRFunction *ir_func, ASTNode *func)
{
	StringView func_name = func->function.name;
	size_t param_count = func->function.param_count;

	JITResult result = {0};
	reset_state(ctx);

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
	result.code = arena_alloc_aligned(ctx->exec_arena, predicted_size, 16);
    if (!result.code)
	{
		fprintf(stderr, BOLD_RED "  > failed to allocate %zu bytes for JIT code\n" RESET,
				predicted_size);
	}

	// === PASS 2: Emit ===
	reset_state(ctx);
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

bool	jit_compile_pass(JITContext *jit_ctx, CompilationContext *comp_ctx, 
					ErrorContext *errors)
{
	for (size_t i = 0; i < comp_ctx->count; ++i)
	{
		CompilationUnit *unit = &comp_ctx->units[i];
		if (!unit->parsed_ok)
			continue;

		for (size_t j = 0; j < unit->ast->translation_unit.count; ++j)
		{
			ASTNode *func = unit->ast->translation_unit.declarations[j];
			if (func->function.is_prototype)
                continue;
			printf("  :: compiling symbol '%.*s'\n", (int)func->function.name.len, func->function.name.start);

			IRFunction *ir = ir_gen(jit_ctx->data_arena, func);
			if (!ir)
			{
				fprintf(stderr,  BOLD_RED "  > ir generation failed\n" RESET);
				error_fatal(errors, unit->file.name, func->line, 0,
						"IR Generation failed for function '%.*s'",
						(int)func->function.name.len, func->function.name.start);
				return (false);
			}

			if (sv_eq_cstr(func->function.name, "main"))
				ir_print(ir);

			JITResult jit = jit_compile_function(jit_ctx, ir, func);
			if (!jit.code)
			{
				fprintf(stderr, BOLD_RED "  > compilation failed\n" RESET);
				error_fatal(errors, unit->file.name, func->line, 0,
						"JIT compilation failed for function '%.*s'",
						(int)func->function.name.len, func->function.name.start);
				return (false);
			}
		}
	}
	return (true);
}
