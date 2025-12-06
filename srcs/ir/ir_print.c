#include "ir.h"
#include <stdio.h>

void ir_print(IRFunction *f)
{
	printf("--- IR CODE ---\n");
	IRChunk *curr = f->head;
	int idx = 0;

	while (curr)
	{
		for (size_t i = 0; i < curr->count; ++i)
		{
			IRInstruction *inst = &curr->instructions[i];
			printf("%04d: ", idx++);
			switch (inst->opcode)
			{
                case IR_CONST: 
                    printf("%%v%zu = CONST %lld\n", inst->dest, inst->imm); 
                    break;
                case IR_ADD: 
                    printf("%%v%zu = ADD %%v%zu, %%v%zu\n", inst->dest, inst->src_1, inst->src_2); 
                    break;
                case IR_SUB: 
                    printf("%%v%zu = SUB %%v%zu, %%v%zu\n", inst->dest, inst->src_1, inst->src_2); 
                    break;
                case IR_MUL: 
                    printf("%%v%zu = MUL %%v%zu, %%v%zu\n", inst->dest, inst->src_1, inst->src_2); 
                    break;
                case IR_DIV: 
                    printf("%%v%zu = DIV %%v%zu, %%v%zu\n", inst->dest, inst->src_1, inst->src_2); 
                    break;
                case IR_RET:
                    printf("RET %%v%zu\n", inst->src_1);
                    break;
                default: 
                    printf("UNKNOWN\n"); break;
			}
		}
		curr = curr->next;
	}
	printf("--- END ---\n");
}
