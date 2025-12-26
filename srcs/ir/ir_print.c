#include "ir.h"
#include <stdio.h>
#include "layout.h"

#define INST_BUF_SIZE 128

static void	print_jit_ir_box_start(void);
static void	print_jit_ir_line(size_t line_num, const char* instruction);
static void	print_jit_ir_box_end(void);

// 1. Generate name table
static const char	*opcode_names[] =
{
	#define X_OP(opcode, name, fmt, encoder) name,
	#include "ir_ops.def"
	#undef X_OP
};

// 2. Generate format table
static const IROpcodeFormat	opcode_formats[] =
{
	#define X_OP(opcode, name, fmt, encoder) fmt,
	#include "ir_ops.def"
	#undef X_OP
};

const char	*ir_opcode_name(IROpcode op)
{
	return (opcode_names[op]);
}

IROpcodeFormat	ir_opcode_format(IROpcode op)
{
	return (opcode_formats[op]);
}

static void	format_instruction(char *buf, size_t buf_size, IRInstruction *inst)
{
	IROpcodeFormat	fmt = ir_opcode_format(inst->opcode);
	const char		*name = ir_opcode_name(inst->opcode);
	
	switch (fmt)
	{
		case FMT_BIN:
			snprintf(buf, buf_size, "%%v%zu = %s %%v%zu, %%v%zu",
				inst->dest, name, inst->src_1, inst->src_2);
			break;
		case FMT_UNARY:
			if (inst->opcode == IR_RET)
				snprintf(buf, buf_size, "%s %%v%zu", name, inst->src_1);
			else
				snprintf(buf, buf_size, "%%v%zu = %s %%v%zu",
					inst->dest, name, inst->src_1);
			break;
		case FMT_IMM:
			snprintf(buf, buf_size, "%%v%zu = %s %lld",
				inst->dest, name, inst->imm);
			break;
		case FMT_ARG:
			snprintf(buf, buf_size, "%s %lld = %%v%zu",
				name, inst->imm, inst->src_1);
			break;
		case FMT_CALL:
			snprintf(buf, buf_size, "%%v%zu = %s %.*s",
				inst->dest, name, (int)inst->func_name.len, inst->func_name.start);
			break;
		case FMT_LABEL:
			snprintf(buf, buf_size, "L%zu:", inst->label_id);
			break;
		case FMT_JUMP:
			snprintf(buf, buf_size, "%s L%zu", name, inst->label_id);
			break;
		case FMT_BRANCH:
			snprintf(buf, buf_size, "%s %%v%zu, L%zu", name, inst->src_1, inst->label_id);
			break;
		default:
			snprintf(buf, buf_size, "UNKNOWN(%d)", inst->opcode);
			break;
	}
}

void ir_print(IRFunction *f)
{
	if (!f)
		return;
	print_jit_ir_box_start();
	IRChunk *curr = f->head;
	int idx = 0;
	char buffer[INST_BUF_SIZE];

	while (curr)
	{
		for (size_t i = 0; i < curr->count; ++i)
		{
			format_instruction(buffer, INST_BUF_SIZE, &curr->instructions[i]);
			print_jit_ir_line(idx, buffer);
			idx++;
		}
		curr = curr->next;
	}
	print_jit_ir_box_end();
}

static void print_jit_ir_box_start(void)
{
	char *text = " IR DUMP ";
	size_t len = strlen(text);

	printf("\n  " CYAN BOX_TL);
	printf(BOX_H);
	printf("%s", text);
	for (size_t i = 0; i < 39 - len - 1; ++i)
		printf(BOX_H);
	printf(BOX_TR RESET "\n");
}

static void print_jit_ir_line(size_t line_num, const char* instruction)
{
	printf("  " CYAN BOX_V RESET " %04zu | %-30s " CYAN BOX_V RESET "\n", line_num, instruction);
}

static void print_jit_ir_box_end(void)
{
	printf("  " CYAN BOX_BL);
	for (int i = 0; i < 39; ++i)
		printf(BOX_H);
	printf(BOX_BR RESET "\n");
}
