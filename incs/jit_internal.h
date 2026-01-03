#ifndef JIT_INTERNAL_H
# define JIT_INTERNAL_H

# include "ir.h"
# include "jit.h"
# include "defines.h"

/* --- Shared Globals defined in jit.c --- */
extern const X86Reg arg_registers[10];

/* --- Shared Helpers --- */
Location get_location(JITContext *ctx, size_t vreg);

/* --- Location Helper Functions -- */
void	load_location_to_reg(uint8_t **buf, size_t *size, X86Reg dst, Location loc);
void	store_reg_to_location(uint8_t **buf, size_t *size, Location loc, X86Reg src);
void	load_binary_operands(uint8_t **buf, size_t *size, X86Reg dst, X86Reg tmp,
							Location left, Location right, X86Reg *out_right_reg);
size_t	emit_standard_binary_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx,
							X86Opcode opcode);
size_t	emit_comparison_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx,
							X86Condition condition);
size_t		emit_shift_op(uint8_t *buf, IRInstruction *inst, JITContext *ctx, 
							X86Extension extension);

/* --- Encoder Prototypes (Auto-Generated) --- */
#define X_OP(opcode, name, fmt, encoder) \
	size_t encoder(uint8_t *buf, size_t *cnt, IRInstruction *inst, JITContext *ctx);
#include "ir_ops.def"
#undef X_OP

#endif
