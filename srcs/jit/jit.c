#include "jit.h"
#include "compile.h"
#include "defines.h"
#include "jit_internal.h"
#include "ir.h"
#include "layout.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

// Calling convention
const X86Reg arg_registers[10] = {
	REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9,
	REG_R12, REG_R13, REG_R14, REG_R15
};

/* ===================== */
/* LINEAR SCAN ALLOCATOR */
/* ===================== */

/* Strict policy; allocate only Callee-Saved registers. */
static bool is_allocatable_register(int reg)
{
	if (reg == REG_RBX || (reg >= REG_R12 && reg <= REG_R15))
		return (true);
	return (false);
}

Location get_location(JITContext *ctx, size_t vreg)
{
	if (ctx->vreg_map[vreg].type != 0)
		return (ctx->vreg_map[vreg]);

	for (int i = 0; i < 16; ++i)
	{
		if (!is_allocatable_register(i))
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
	// Place locals below the callee-saved spill area (rbp - CALLEE_SAVED_SIZE)
	// and below the fixed 8-byte pad used to restore 16-byte alignment for calls.
	int32_t slot_idx = ctx->stack_base + vreg;
	ctx->vreg_map[vreg].offset = -(CALLEE_SAVED_SIZE + 8 + (int32_t)(slot_idx + 1) * 8);
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

static size_t encode_prologue(uint8_t *buf, size_t stack_size, size_t param_count, JITContext *ctx)
{
	uint8_t	*curr = buf;
	size_t	size = 0;

	// 1. Standard prologue (Push RBP, move RBP, sub RSP)
	emit_push(&curr, &size, REG_RBP);
	emit_mov_reg_reg(&curr, &size, REG_RBP, REG_RSP);

	// 2. Save all callee-saved registers
	emit_push(&curr, &size, REG_RBX);
	emit_push(&curr, &size, REG_R12);
	emit_push(&curr, &size, REG_R13);
	emit_push(&curr, &size, REG_R14);
	emit_push(&curr, &size, REG_R15);

	// 3. Allocate stack for locals
	// Always reserve align_pad so that RSP is 16-byte aligned before any CALL
	if (stack_size > 0)
	{
		emit_u8(&curr, &size, REX_W);
		emit_u8(&curr, &size, ALU_IMM);
		emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
		emit_u32(&curr, &size, (uint32_t)stack_size);
	}

	// 4. Handle arguments
	for (size_t i = 0; i < param_count; ++i)
	{
		Location loc = get_location(ctx, i);
		if (i < 6)
		{
			X86Reg src_reg = arg_registers[i];
			if (loc.type == LOC_REG && loc.reg != src_reg)
				emit_mov_reg_reg(&curr, &size, loc.reg, src_reg);
			else if (loc.type == LOC_STACK)
				emit_store_local(&curr, &size, src_reg, loc.offset);
		}
		else
		{
			int32_t src_offset = 16 + (i - 6) * 8;
			if (loc.type == LOC_REG)
				emit_load_param(&curr, &size, loc.reg, src_offset);
			else
			{
				emit_load_param(&curr, &size, REG_RAX, src_offset);
				emit_store_local(&curr, &size, REG_RAX, loc.offset);
			}
		}
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

	// Round locals to 16-byte boundary; align_pad handled inside encode_prologue
	ctx->stack_base = ir_func->stack_count;
	size_t total_slots = ir_func->stack_count + ir_func->vreg_count;
	size_t stack_bytes = total_slots * 8;
	if ((stack_bytes % 16) == 0)
		stack_bytes += 8;

	// == PASS 1: Calculate size ===
	size_t predicted_size = encode_prologue(NULL, stack_bytes, param_count, ctx);
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
		fprintf(stderr, BOLD_RED "	> failed to allocate %zu bytes for JIT code\n" RESET,
				predicted_size);
	}

	// === PASS 2: Emit ===
	reset_state(ctx);
	uint8_t *write_ptr = result.code;
	size_t prologue_size = encode_prologue(write_ptr, stack_bytes, param_count, ctx);
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
						"    Predicted: %zu bytes\n"
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
		fprintf(stderr,"\n	> IR dump for failed function:\n");
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
				fprintf(stderr, BOLD_RED "	> compilation failed\n" RESET);
				error_fatal(errors, unit->file.name, func->line, 0,
						"JIT compilation failed for function '%.*s'",
						(int)func->function.name.len, func->function.name.start);
				return (false);
			}
		}
	}
	return (true);
}
