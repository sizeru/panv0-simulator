#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "shorttypes.h"
#include "stack.h"

enum Operation {
	OP_PUSHIL,
	OP_PUSHIA,
	OP_PUSHSL,
	OP_PUSHSA,
	OP_PUSHRL,
	OP_PUSHRA,
	OP_ADD,
	OP_JUMPABS,
	OP_JUMP,
	OP__MAX,
};

struct Instruction {
	u32 len;
	Operation op;
	Data data;
};

Instruction getInstr(void *instrPos);
bool runInstr(const Instruction& instr, Stack& execStack, Stack& spmStack, u8 const memory[]);

void printbits(void *insn, unsigned char len);

#endif // INSTRUCTION_H
