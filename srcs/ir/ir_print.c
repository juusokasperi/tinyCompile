#include "ir.h"
#include <stdio.h>
#include "layout.h"

#define INST_BUF_SIZE 128

static void	print_jit_ir_box_start(void);
static void	print_jit_ir_line(size_t line_num, const char* instruction);
static void	print_jit_ir_box_end(void);

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
			IRInstruction *inst = &curr->instructions[i];
			switch (inst->opcode)
			{
                case IR_CONST:
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = CONST %lld", inst->dest, inst->imm);
                    break;
                case IR_ADD: 
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = ADD %%v%zu, %%v%zu", inst->dest, inst->src_1, inst->src_2);
                    break;
                case IR_SUB: 
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = SUB %%v%zu, %%v%zu", inst->dest, inst->src_1, inst->src_2);
                    break;
                case IR_MUL: 
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = MUL %%v%zu, %%v%zu", inst->dest, inst->src_1, inst->src_2);
                    break;
                case IR_DIV: 
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = DIV %%v%zu, %%v%zu", inst->dest, inst->src_1, inst->src_2);
                    break;
				case IR_ARG:
					snprintf(buffer, INST_BUF_SIZE, "ARG %lld = %%v%zu", inst->imm, inst->src_1);
					break;
				case IR_CALL:
					snprintf(buffer, INST_BUF_SIZE, "%%v%zu = CALL %.*s", inst->dest,
                    		(int)inst->func_name.len, inst->func_name.start);
					break;
                case IR_RET:
					snprintf(buffer, INST_BUF_SIZE, "RET %%v%zu", inst->src_1);
                    break;
                default: 
					snprintf(buffer, INST_BUF_SIZE, "UNKNOWN");
					break;
			}
			print_jit_ir_line(idx++, buffer);
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
    printf(BOX_BR RESET "\n\n");
}
