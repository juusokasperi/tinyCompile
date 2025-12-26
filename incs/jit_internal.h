#ifndef JIT_INTERNAL_H
# define JIT_INTERNAL_H

# include "jit.h"
# include "defines.h"

/* --- Shared Globals defined in jit.c --- */
extern const X86Reg arg_registers[6];

/* --- Shared Helpers --- */
Location get_location(JITContext *ctx, size_t vreg);

/* --- Encoder Prototypes (Auto-Generated) --- */
#define X_OP(opcode, name, fmt, encoder) \
	size_t encoder(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx);
#include "ir_ops.def"
#undef X_OP

#endif
