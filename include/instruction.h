#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "shorttypes.h"
#include "data.h"

enum Operation {
	OP_PREP,
	OP_PUSHI,
	OP_PUSHS,
	OP_PUSHR,
	OP_ADD,
	OP_JUMPABS,
	OP_JUMP,
	OP__MAX,
};

static const char* opNames[OP__MAX] = {
	"PREP", "PUSH.I", "PUSH.S", "PUSH.R", "ADD", "JUMP.ABS", "JUMP"
};

struct Instruction {
	Operation op;
	Data data;
	u32 len;
};

Instruction getInstr(void *instrPos);

bool runInstr(const Instruction& instr, Stack& prepStack, Stack& execStack, Stack& spmStack, u8 memory[], Regfile& regs);

void printbits(void *insn, unsigned char len);

#endif // INSTRUCTION_H
