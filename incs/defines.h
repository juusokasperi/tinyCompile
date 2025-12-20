#ifndef DEFINES_H
# define DEFINES_H

# define MAX_SOURCE_FILES			64
# define IR_CHUNK_SIZE				64 
# define MAX_PARAMS_PER_FUNCTION	32
# define MAX_VARS_PER_SCOPE			256
# define MAX_FUNCTION_COUNT			256
# define MAX_CALL_SITES				1024
# define SYMBOL_TABLE_SIZE			4096
# define MAX_LABELS					512
# define MAX_BLOCK_STATEMENTS		512
# define MAX_EXPRESSION_DEPTH		128

#define CHECK_LIMIT(value, limit, name) \
    do { \
        if ((value) >= (limit)) { \
            fprintf(stderr, "Limit exceeded: %s (got %zu, max %zu)\n", \
                    name, (size_t)(value), (size_t)(limit)); \
            return false; \
        } \
    } while (0)

#define CHECK_LIMIT_NULL(value, limit, name) \
    do { \
        if ((value) >= (limit)) { \
            fprintf(stderr, "Limit exceeded: %s (got %zu, max %zu)\n", \
                    name, (size_t)(value), (size_t)(limit)); \
            return NULL; \
        } \
    } while (0)

// Static assertions to ensure sane defaults
_Static_assert((SYMBOL_TABLE_SIZE & (SYMBOL_TABLE_SIZE - 1)) == 0, 
               "SYMBOL_TABLE_SIZE must be power of 2");
_Static_assert(MAX_PARAMS_PER_FUNCTION <= 255,
               "MAX_PARAMS_PER_FUNCTION must fit in uint8_t");

#endif
