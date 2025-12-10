#include "jit.h"
#include <stdio.h>
#include <string.h>
#include "layout.h"

// Calling convention
static const X86Reg arg_registers[6] = {
	REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9,
};

static PendingCall pending_call = {0};

static int32_t get_slot(size_t vreg)
{
	return -((int32_t)(vreg + 1) * 8);
}

static size_t encode_prologue(uint8_t *buf, size_t stack_size, size_t param_count)
{
	uint8_t *curr = buf;
	size_t size = 0;

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

static size_t encode_inst(uint8_t *buf, IRInstruction *inst, CallSiteList *call_sites)
{
	uint8_t *curr = buf;
	size_t size = 0;

	int32_t dest_disp = get_slot(inst->dest);
	int32_t src_1_disp = get_slot(inst->src_1);
	int32_t src_2_disp = get_slot(inst->src_2);
	switch (inst->opcode)
	{
		case IR_ADD:
			emit_load_param(&curr, &size, REG_RAX, src_1_disp);
			emit_load_param(&curr, &size, REG_RCX, src_2_disp);
			emit_alu(&curr, &size, ALU_ADD, REG_RAX, REG_RCX);
			emit_store_local(&curr, &size, REG_RAX, dest_disp);
			break;
        case IR_SUB:
			emit_load_param(&curr, &size, REG_RAX, src_1_disp);
			emit_load_param(&curr, &size, REG_RCX, src_2_disp);
			emit_alu(&curr, &size, ALU_SUB, REG_RAX, REG_RCX);
			emit_store_local(&curr, &size, REG_RAX, dest_disp);
			break;
        case IR_MUL:
			emit_load_param(&curr, &size, REG_RAX, src_1_disp);
			emit_load_param(&curr, &size, REG_RCX, src_2_disp);
			emit_imul_r64(&curr, &size, REG_RAX, REG_RCX);
			emit_store_local(&curr, &size, REG_RAX, dest_disp);
            break;
		case IR_DIV:
			emit_load_param(&curr, &size, REG_RAX, src_1_disp);
			emit_u8(&curr, &size, REX_W);
			emit_u8(&curr, &size, OP_CQO);
			emit_load_param(&curr, &size, REG_RCX, src_2_disp);
			emit_u8(&curr, &size, REX_W);
			emit_u8(&curr, &size, OP_IDIV);
			emit_u8(&curr, &size, MOD_REG | (EXT_IDIV << 3) | REG_RCX);
			emit_store_local(&curr, &size, REG_RAX, dest_disp);
			break;
		case IR_CONST:
			emit_mov_imm(&curr, &size, REG_RAX, inst->imm);
			emit_store_local(&curr, &size, REG_RAX, dest_disp);
			break;
		case IR_ARG:
			pending_call.arg_vregs[pending_call.count++] = inst->src_1;
			break;
		case IR_CALL:
		{
			for (size_t i = 0; i < pending_call.count; ++i)
			{
				int32_t arg_disp = get_slot(pending_call.arg_vregs[i]);
				if (i < 6) // Load into calling convetion register
					emit_load_param(&curr, &size, arg_registers[i], arg_disp);
				else // Push to stack for args 7+
				{
					emit_load_param(&curr, &size, REG_RAX, arg_disp);
					emit_u8(&curr, &size, OP_PUSH + REG_RAX);
				}
			}
			size_t stack_args = (pending_call.count > 6) ? (pending_call.count - 6) : 0;
			bool needs_alignment = (stack_args % 2) != 0;
			if (needs_alignment) // sub rsp, 8
			{
				emit_u8(&curr, &size, REX_W);
				emit_u8(&curr, &size, ALU_IMM);
				emit_u8(&curr, &size, MOD_REG | (EXT_SUB << 3) | REG_RSP);
				emit_u32(&curr, &size, 8);
			}

			// Load function address into RAX 
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
			emit_u64(&curr, &size, 0xDEADBEEFDEADBEEF);  // Placeholder

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
			break;
		}
        case IR_RET:
			emit_load_param(&curr, &size, REG_RAX, src_1_disp);
			emit_u8(&curr, &size, REX_W);
			emit_u8(&curr, &size, MOV_RM_R);
			emit_u8(&curr, &size, MOD_REG | (REG_RBP << 3) | REG_RSP);
			emit_u8(&curr, &size, OP_POP + REG_RBP);
			emit_u8(&curr, &size, OP_RET);
            break;
            
        default: break;
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
