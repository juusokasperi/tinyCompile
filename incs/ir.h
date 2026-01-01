#ifndef IR_H
# define IR_H

#include "error_handler.h"
# include "memarena.h"
# include "ast.h"
# include "string_view.h"
# include "defines.h"
# include <stdbool.h>
# include <stdlib.h>
# include <stddef.h>

typedef struct ScopeChange ScopeChange;

typedef struct {
	StringView	name;
	size_t		index;
	bool		is_stack;
	bool		occupied;
} Symbol;

struct ScopeChange {
	size_t		index;
	Symbol		previous;
	ScopeChange	*next;
};

typedef struct {
	Symbol		entries[SYMBOL_TABLE_SIZE];
	Arena		*arena;
	ScopeChange	*changes;
} SymbolTable;

typedef enum {
	FMT_NONE,
	FMT_BIN,	// dest = src_1 op src_2
	FMT_UNARY,	// dest = op src_1
	FMT_IMM,	// dest = imm
	FMT_CALL,	// dest = call name
	FMT_ARG,	// arg imm = src_1
	FMT_JUMP,	// jmp label
	FMT_BRANCH,	// op src_1, label
	FMT_LABEL	// label1:
}	IROpcodeFormat;

typedef enum {
	#define X_OP(opcode, name, fmt, encoder) opcode,
	#include "ir_ops.def"
	#undef X_OP
}	IROpcode;

typedef struct {
	IROpcode	opcode;
	DataType	type;
	size_t		dest;	// Virtual register ID 
	size_t		src_1;
	size_t		src_2;
	int64_t		imm;	// Immediate value for IR_CONST
	StringView	func_name;
	size_t		label_id;
} IRInstruction;

typedef struct IRChunk IRChunk;

struct IRChunk {
	IRChunk			*next;
	IRInstruction	instructions[IR_CHUNK_SIZE];
	size_t			count;
};

typedef struct {
	IRChunk			*head;
	IRChunk			*tail;
	size_t			total_count;
	size_t			vreg_count;
	size_t			stack_count;
	size_t			label_count;
	StringView		name;
	ErrorContext	*errors;
	const char		*filename;
} IRFunction;

void			symbol_table_restore(SymbolTable *st, ScopeChange *target_state);
Symbol*			symbol_table_lookup(SymbolTable *st, StringView name);
void			symbol_table_add(SymbolTable *st, StringView name, size_t vreg);

IRFunction		*ir_gen(Arena *a, ASTNode *root, 
						ErrorContext *errors, const char *filename);
void			ir_print(IRFunction *func);

const char		*ir_opcode_name(IROpcode op);
IROpcodeFormat	ir_opcode_format(IROpcode op);

static inline bool ir_alloc_vreg(IRFunction *f, size_t *out_vreg)
{
    if (f->vreg_count >= MAX_VREGS_PER_FUNCTION)
    {
        if (f->errors)
        {
            error_add(f->errors, ERROR_CODEGEN, ERROR_LEVEL_ERROR,
                    f->filename, 0, 0,
                    "function '%.*s' exceeds virtual register limit (%d)",
                    (int)f->name.len, f->name.start, MAX_VREGS_PER_FUNCTION);
        }
        return false;
    }
    *out_vreg = f->vreg_count++;
    return true;
}

#endif
