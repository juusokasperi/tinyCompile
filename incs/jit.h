#ifndef JIT_H
# define JIT_H

# include <stdbool.h>
# include <stdint.h>
# include <stddef.h>
# include "defines.h"
# include "string_view.h"
# include "ir.h"
# include "memarena.h"
# include "error_handler.h"
# include "compile.h"

typedef enum {
    REG_RAX = 0,
    REG_RCX = 1,
    REG_RDX = 2,
    REG_RBX = 3,
    REG_RSP = 4,
    REG_RBP = 5,
    REG_RSI = 6,
    REG_RDI = 7,
	REG_R8 = 8,
	REG_R9 = 9,
} X86Reg;

typedef enum {
	REX_B = 0x40,
	REX_W = 0x48,	// 64-bit Operand Size
	REX_WB = 0x49,	// REX.W + REX_B (for R8-R15)
} X86Prefix;

typedef enum {
	CC_E = 0x4,		// Equal / Zero
	CC_NE = 0x5,	// Not Equal / Not Zero
	CC_L = 0xC,		// Less
	CC_GE = 0xD,	// Greater or Equal
	CC_LE = 0xE,	// Less or equal
	CC_G = 0xF,		// Greater
} X86Condition;

typedef enum {
	ALU_ADD = 0x01,
	ALU_OR = 0x09,
	ALU_AND = 0x21,
	ALU_SUB = 0x29,
	ALU_XOR = 0x31,
	ALU_CMP = 0x39,
	ALU_IMM = 0x81,		// 32-bit immediate
	ALU_IMM8 = 0x83,	// Add/Cmp/Sub r/m, imm8
	ALU_TEST = 0x85,

	MOV_RM_R = 0x89,	// Store: Move register to r/m
	MOV_R_RM = 0x8B,	// Load: Move r/m to register
	MOV_IMM_R = 0xB8,	// Imm: Mov imm64 to register
	
	OP_PUSH = 0x50,		// Push reg
	OP_POP = 0x58,
	OP_RET = 0xC3,
	OP_LEAVE = 0xC9,	// Leave (mov rsp, rbp; pop rbp)
	OP_CALL_IND = 0xFF,	// Call indirect (call rax)
	
	OP_JMP_REL32 = 0xE9,	// JMP rel32
	OP_PREFIX_0F = 0x0F,	// Prefix for 2-byte opcodes
	OP_MOVZX = 0xB6,		// MOVZX r64, r/m8 (w/ 0f prefix)
	OP_LEA = 0x8D,			// Load effective address

	OP_CQO = 0x99,		// Sign extend (RAX -> RDX)
	OP_IDIV = 0xF7,		// Integer division
	OP_IMUL_1 = 0x0F,
	OP_IMUL_2 = 0xAF,	// 2nd byte of IMUL (1st is 0x0F)
} X86Opcode;

typedef enum {
	MOD_MEM = 0x00,			// [reg]
	MOD_MEM_DISP8 = 0x40, 	// [reg + disp8]
	MOD_MEM_DISP32 = 0x80, 	// [reg + disp32]
	MOD_REG = 0xC0,			// reg (direct register access)
} X86ModMode;

typedef enum {
	EXT_ADD = 0,
	EXT_CALL = 2,
	EXT_NEG = 3,
	EXT_SUB = 5,
	EXT_IDIV = 7,
	EXT_CMP = 7,
} X86Extension;

typedef enum {
	LOC_NONE = 0,
	LOC_REG,
	LOC_STACK,
	LOC_CONST
} LocationType;

typedef struct {
	LocationType type;
	union 
	{
		X86Reg	reg;
		int32_t	offset;
		int64_t	imm;
	};
} Location;

typedef struct {
	uint8_t *code;
	size_t size;
} JITResult;

typedef int64_t (*JITFunc)(void);

typedef struct Patch Patch;

struct Patch {
	size_t	label_id;
	uint8_t	*loc;
	Patch	*next;
};

typedef struct {
	StringView	name;
	uint8_t		*code_addr;
	size_t		code_size;
} CompiledFunction;

typedef struct {
	CompiledFunction	*functions;
	size_t				count;
	size_t				capacity;
} FunctionRegistry;

typedef struct {
	uint8_t		*patch_location;
	StringView	target_name;
} CallSite;

typedef struct {
	CallSite	*sites;
	size_t		count;
	size_t		capacity;
} CallSiteList;

typedef struct {
	size_t	arg_vregs[MAX_PARAMS_PER_FUNCTION];
	size_t	count;
} PendingCall;

typedef struct {
	Arena 				*data_arena;
	Arena				*exec_arena;
	FunctionRegistry	registry;
	CallSiteList		call_sites;
	PendingCall			pending_call;

	uint8_t				*label_offset[MAX_LABELS];
	bool				label_defined[MAX_LABELS];
	Patch				*patches;

	Location			*vreg_map;
	bool				phys_regs[16];
} JITContext;

bool	jit_compile_pass(JITContext *jit_ctx, CompilationContext *comp_ctx, 
					ErrorContext *errors);
void		jit_ctx_init(JITContext *ctx, Arena *a, Arena *exec_arena);
JITResult	jit_compile_function(JITContext *ctx, IRFunction *ir_func, ASTNode *func);
bool		jit_link_all(JITContext *ctx, ErrorContext *errors);

void		emit_u8(uint8_t **buf, size_t *count, uint8_t byte);
void		emit_u32(uint8_t **buf, size_t *count, uint32_t val);
void		emit_u64(uint8_t **buf, size_t *count, uint64_t val);
void		emit_store_local(uint8_t **buf, size_t *cnt, X86Reg src, int32_t disp);
void		emit_load_param(uint8_t **buf, size_t *cnt, X86Reg dst, int32_t disp);
void		emit_mov_imm(uint8_t **buf, size_t *cnt, X86Reg dst, uint64_t imm);
void		emit_alu(uint8_t **buf, size_t *cnt, X86Opcode op, X86Reg dst, X86Reg src);
void		emit_imul_r64(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src);
void        emit_mov_reg_reg(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src);
void		emit_cmp(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src);
void		emit_movzx(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src);
void		emit_setcc(uint8_t **buf, size_t *cnt, X86Condition cc, X86Reg dst);
void		emit_test(uint8_t **buf, size_t *cnt, X86Reg dst, X86Reg src);
void		emit_pop(uint8_t **buf, size_t *cnt, X86Reg reg);
void		emit_push(uint8_t **buf, size_t *cnt, X86Reg reg);

#endif
