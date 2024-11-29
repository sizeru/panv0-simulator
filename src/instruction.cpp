#include "instruction.h"
#include "data.h"
#include <cstring>
#include <stdio.h>

// For debugging purposes
void printbits(void* insn, unsigned char len) {
	insn = (u8*)insn + len - 1;
	for (int i = len - 1; i >= 0; i--) {
		u8 byte = *(u8*)insn;
		for (int i = 7; i >= 4; i--) putchar('0' + ((byte >> i) & 1));
		printf("_");
		for (int i = 3; i >= 0; i--) putchar('0' + ((byte >> i) & 1));
		insn = (u8*)insn - 1;
		if (i - 1 >= 0) printf(" ");
	}
}

// Count trailing ones of an arbitrary data type
u16 cto(const void* insn) {
	u16 ones_count = 0;
	while (1) {
		u8 byte = *(u8*)insn;
		for (int i = 0; i < 8; i++) {
			if (!(byte & (1 << i))) {
				return ones_count;
			}
			ones_count++;
		}
		insn = (u8*)insn + 1;
	}
}

// Bit field extract
u64 bfe(u64 bitfield, u8 startPos, u8 length) {
	return (bitfield >> startPos) & ((1UL << length) - 1);
}

void getInstrLen(void* instr, u32& instrLen, u32& h) {
	h = cto(instr) + 1;
	if (h == 1) {
		instrLen = 1;
		return;
	}
	u32 a = bfe(*(u64*)instr, 0, h);
	u32 b = bfe(*(u64*)instr, h, h);
	instrLen = 2 * a + b;
	return;
}

void getOp(
	Operation& op, u32& opLen, void* instr, u32 instrLen, u32 suffixLen
) {
	// Startpos and length are both in bits
}

Instruction::Instruction() : op(OP__MAX), data(), len(0) {}

Instruction::Instruction(u8* memory, Register& pc) :
	op(parseOp(memory, pc)), data(parseData(memory, pc)),
	len(parseLen(memory, pc)) {}

Instruction Instruction::fetch(u8* memory, Register& pc) {
	return Instruction(memory, pc);
}

u16 Instruction::parseLen(const u8* imem, const Register& pc) {
	const void* insnbits = reinterpret_cast<const void*>(imem + pc.val);
	u64 h = cto(insnbits) + 1;
	if (h == 1) {
		return 1;
	}
	u32 a = bfe(*(u64*)insnbits, 0, h);
	u32 b = bfe(*(u64*)insnbits, h, h);
	return 2 * a + b;
}

u16 Instruction::parseSuffixLen(const u8* imem, const Register& pc) {
	bool lsb = *(imem + pc.val) & 1;
	const void* insnbits = reinterpret_cast<const void*>(imem + pc.val);
	return (cto(insnbits) + 1) * (1 + lsb);
}

u16 Instruction::parseOpLen(u16 instrLen) {
	switch (instrLen) {
	case 0: return 0; // Instructions of length 0 makes no sense
	case 1: return 3;
	case 2:
	case 3:
	case 4: return instrLen * 2;
	default: return 2 * (4 + instrLen / 4);
	}
}

// Extracts the op from an instruction
Operation Instruction::parseOp(const u8* imem, const Register& pc) {
	u16 instrLen = parseLen(imem, pc);
	if (instrLen > 8) {
		// NOTE(Nate): Instructions above 8 bytes not currently supported
		return OP__MAX;
	}

	const void* insnbits = reinterpret_cast<const void*>(imem + pc.val);
	u16 opOffset = parseSuffixLen(imem, pc);
	u16 opLen = parseOpLen(instrLen);

	Operation op = (Operation)bfe(*(u64*)insnbits, opOffset, opLen);
	return op;
}

Data Instruction::parseData(const u8* imem, const Register& pc) {
	// TODO(Nate): We should not be sign extending PUSH.I address
	// instructions. This likely means that sign extension should only
	// occur upon executing a non-prep instruction. This is passable for
	// now, since we never push addresses.
	u16 instrLen = parseLen(imem, pc);
	u16 suffixLen = parseSuffixLen(imem, pc);
	u16 opLen = parseOpLen(instrLen);

	Data data(
		reinterpret_cast<const void*>(imem + pc.val),
		suffixLen + opLen,
		instrLen * 8 - (suffixLen + opLen)
	);
	// printf("\tData was %ub. Extended to %uB. (", instrLen*8-(suffixLen+opLen),
	// data.length); printbits(&data.val, data.length); printf(")\n");
	return data;

	// u64 debugVal = data.val;
	// if (shouldSignExtend) {
	// 	signExtend(reinterpret_cast<u8*>(&debugVal), data.length*8, 64);
	// }
	// printf("\t[ %s %li ]\n", opNames[op], debugVal);
	// return Instruction{op, data, instrLen};
}
