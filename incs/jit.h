#ifndef JIT_H
# define JIT_H

# include <stdint.h>
# include "ir.h"
# include "memarena.h"

typedef struct {
	uint8_t *code;
	size_t size;
} JITResult;

typedef int64_t (*JITFunc)(void);

JITResult jit_compile(Arena *a, IRFunction *func);

#endif
