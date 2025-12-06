#ifndef IR_H
# define IR_H

# include "memarena.h"
# include "ast.h"
# include "shared_types.h"

typedef struct {
	StringView	name;
	size_t		vreg;
} Symbol;

typedef struct {
	Symbol	symbols[256];
	size_t	count;
} SymbolTable;

typedef enum {
	IR_CONST,	// immediate value (x = 5)
	IR_ADD,
	IR_SUB,
	IR_MUL,
	IR_DIV,

	IR_NEG,
	IR_NOT,

	IR_RET,
} IROpcode;

typedef struct {
	IROpcode	opcode;
	size_t		dest;	// Virtual register ID 
	size_t		src_1;
	size_t		src_2;
	int64_t		imm;	// Immediate value for IR_CONST
} IRInstruction;

# define IR_CHUNK_SIZE 64 

typedef struct IRChunk IRChunk;

struct IRChunk {
	IRChunk			*next;
	IRInstruction	instructions[IR_CHUNK_SIZE];
	size_t			count;
};

typedef struct {
	IRChunk	*head;
	IRChunk	*tail;
	size_t	total_count;
	size_t	vreg_count;
} IRFunction;

Symbol* symtab_lookup(SymbolTable *st, StringView name);
void symtab_add(SymbolTable *st, StringView name, size_t vreg);

IRFunction	*ir_gen(Arena *a, ASTNode *root);
void ir_print(IRFunction *func);

#endif
