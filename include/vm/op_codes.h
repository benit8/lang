#pragma once

#include <stdint.h>

#define __ENUMERATE_OP_CODES \
	__ENUMERATE(NOP)         \
	__ENUMERATE(PUSH)        \
	__ENUMERATE(PUSH_FALSE)  \
	__ENUMERATE(PUSH_TRUE)   \
	__ENUMERATE(PUSH_CONST)  \
	__ENUMERATE(LOAD)        \
	__ENUMERATE(STORE)       \
	__ENUMERATE(LOAD_UP)     \
	__ENUMERATE(STORE_UP)    \
	__ENUMERATE(ADD)         \
	__ENUMERATE(SUB)         \
	__ENUMERATE(MUL)         \
	__ENUMERATE(DIV)         \
	__ENUMERATE(MOD)         \
	__ENUMERATE(POW)         \
	__ENUMERATE(INC)         \
	__ENUMERATE(DEC)         \
	__ENUMERATE(NEG)         \
	__ENUMERATE(EQ)          \
	__ENUMERATE(NEQ)         \
	__ENUMERATE(GT)          \
	__ENUMERATE(GTE)         \
	__ENUMERATE(LT)          \
	__ENUMERATE(LTE)         \
	__ENUMERATE(CMP)         \
	__ENUMERATE(AND)         \
	__ENUMERATE(OR)          \
	__ENUMERATE(NOT)         \
	__ENUMERATE(BAND)        \
	__ENUMERATE(BOR)         \
	__ENUMERATE(BNOT)        \
	__ENUMERATE(XOR)         \
	__ENUMERATE(LSH)         \
	__ENUMERATE(RSH)         \
	__ENUMERATE(GETG)        \
	__ENUMERATE(GETP)        \
	__ENUMERATE(CLOSE)       \
	__ENUMERATE(CALL)        \
	__ENUMERATE(RETURN)      \
	__ENUMERATE(JUMP)        \
	__ENUMERATE(JUMP_IF)     \
	__ENUMERATE(MAKE_ARRAY)  \
	__ENUMERATE(MAKE_TABLE)  \


typedef enum op_code {
#define __ENUMERATE(o) OP_ ## o,
	__ENUMERATE_OP_CODES
#undef __ENUMERATE
} op_code_t;

typedef struct op {
	op_code_t op;
	int16_t arg;
} op_t;
