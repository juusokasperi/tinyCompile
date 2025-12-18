#ifndef IR_H
# define IR_H

# include "memarena.h"
# include "ast.h"
# include "string_view.h"
# include "defines.h"

typedef struct {
	StringView	name;
	size_t		vreg;
} Symbol;

typedef struct {
	Symbol	symbols[MAX_SYMBOLS];
	size_t	count;
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
} 	IROpcode;

typedef struct {
	IROpcode	opcode;
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
	IRChunk		*head;
	IRChunk		*tail;
	size_t		total_count;
	size_t		vreg_count;
	StringView	name;
} IRFunction;

Symbol*			symtab_lookup(SymbolTable *st, StringView name);
void			symtab_add(SymbolTable *st, StringView name, size_t vreg);

IRFunction		*ir_gen(Arena *a, ASTNode *root);
void			ir_print(IRFunction *func);

const char		*ir_opcode_name(IROpcode op);
IROpcodeFormat	ir_opcode_format(IROpcode op);

#endif
