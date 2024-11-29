#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "data.h"
#include "regfile.h"
#include "shorttypes.h"

enum Operation {
	OP_DSIZE,
	OP_PUSHI,
	OP_PUSHS,
	OP_PUSHR,
	OP_ADD,
	OP_JUMPABS,
	OP_JUMP,
	OP__MAX,
};

static const char* opNames[OP__MAX] = {
	"DSIZE", "PUSH.I", "PUSH.S", "PUSH.R", "ADD", "JUMP.ABS", "JUMP"
};

struct Instruction {
	Operation op;
	Data data;
	u32 len;

	Instruction();
	Instruction(u8* mem, Register& pc);

	static Instruction fetch(u8* mem, Register& pc);
	static u16 parseSuffixLen(const u8* mem, const Register& pc);
	static u16 parseLen(const u8* mem, const Register& pc);
	static Operation parseOp(const u8* mem, const Register& pc);
	static u16 parseOpLen(u16 instrLen);
	static Data parseData(const u8* mem, const Register& pc);
};

void printbits(void* insn, unsigned char len);

u64 bfe(u64 bitfield, u8 startPos, u8 length);

#endif // INSTRUCTION_H
