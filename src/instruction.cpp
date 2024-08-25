#include <stdio.h>
#include "instruction.h"
#include "data.h"

// For debugging purposes
void printbits(unsigned char byte) {
    printf("byte: ");
    for (int i = 7; i >= 4; i--) putchar('0' + ((byte >> i) & 1));
    printf("_");
    for (int i = 3; i >= 0; i--) putchar('0' + ((byte >> i) & 1));
    printf("\n");
}

// Count trailing ones of an arbitrary data type
u16 cto(void *insn) {
	u16 ones_count = 0;
	while (1) {
		u8 byte = *(u8 *)insn;
        printbits(byte);
		for (int i = 0; i < 8; i++) {
			if (!(byte & (1 << i))) {
				return ones_count;
			}
            ones_count++;
		}
		insn = (u8 *)insn + 1;
	}
}

// Bit field extract
u64 bfe(u64 bitfield, u8 startPos, u8 length) {
	return (bitfield >> startPos) & ((1UL << length)-1);
}

void getInstrLen(void *instr, u32 &instrLen, u32 &h) {
	h = cto(instr) + 1;
	if (h == 1) {
		instrLen = 1;
		return;
	}
	u32 a = bfe(*(u64 *)instr, 0, h);
	u32 b = bfe(*(u64 *)instr, h, h);
	instrLen = 2 * a + b;
	return;
}

void getOp(Operation &op, u32 &opLen, void *instr, u32 instrLen,
					 u32 suffixLen) {
	// Startpos and length are both in bits
	u64 opStartPos = suffixLen;

	switch (instrLen) {
		case 1:
			opLen = 3;
			op = (Operation)bfe(*(u8*)instr, 1, opLen);
			return;
		case 2:
			opLen = 4; // Double check this
			break;
		case 3:
			opLen = 8; // Double check this
			break;
		case 4:
			opLen = 8; // Double check this
			break;
		default:
			opLen = 2 * (4 + instrLen / 4);
			break;
	}

	if (opLen > 8) {
		// Unsupported
		op = OP__MAX;
		return; }

	op = (Operation)bfe(*(u64 *)instr, suffixLen, opLen);
	return;
}

Instruction getInstr(void *instrPos) {
	u32 instrLen, h;
	getInstrLen(instrPos, instrLen, h);
	u32 suffixLen = h == 1 ? h : 2 * h; // If h == 1 then special case
	Operation op;
	u32 opLen;
	getOp(op, opLen, instrPos, instrLen, suffixLen);
	Data data(instrPos, suffixLen, opLen, instrLen);
	u32 dataLen = instrLen * 8 - opLen;
	u32 rounded = (dataLen + 7) / 8;
	switch (op) {
		case OP_PUSHIL:
			data.flags = DATA_IS_INTEGER | DATA_IS_LITERAL;
			break;
		case OP_PUSHIA:
			data.flags = DATA_IS_INTEGER | DATA_IS_ADDRESS;
			break;
		case OP_PUSHSL:
			data.flags = DATA_IS_SPM | DATA_IS_LITERAL;
			break;
		case OP_PUSHSA:
			data.flags = DATA_IS_SPM | DATA_IS_ADDRESS;
			break;
		case OP_PUSHRL:
			data.flags = DATA_IS_REG | DATA_IS_LITERAL;
			break;
		case OP_PUSHRA:
			data.flags = DATA_IS_REG | DATA_IS_ADDRESS;
			break;
		default:
			break;
	}
	printf("Instr: len: %u, op: %u, data: %ld\n", instrLen, op, data.contents.val);
	return Instruction{instrLen, op, data};
}

// Returns true if the instruction was able to be run. False otherwise.
bool runInstr(const Instruction& instr, Stack& execStack, Stack& spmStack, u8 const memory[]) {
    printf("WHAT THE HECK, runInstr\n");
	if (instr.op < 6) {
		return execStack.push(instr.data); // causing segfault
	}
    printf("WHAT THE HECK, runInstr\n");
	switch (instr.op) {
		case OP_ADD: {
			Data resultLen = instr.data;
			Data dst = execStack.get(0);
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			Data shift = execStack.get(3);
			printf("ADD: resLen: %lu, dst: %lu, op1: %lu, op2: %lu, shift: %lu\n",
				resultLen.contents.val, dst.contents.val, op1.contents.val, op2.contents.val, shift.contents.val
			);
			break;
		}
		case OP_JUMPABS: {
			Data condCode = instr.data;
			Data target = execStack.get(0); // where to jump to
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			printf("ADD: cond: %lu, dst: %li, op1: %li, op2: %lu\n",
				condCode.contents.val, target.contents.val, op1.contents.val, op2.contents.val
			);
			break;
		}
		case OP_JUMP: {
			Data offset = instr.data;
			Data dst = execStack.get(0);
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			Data shift = execStack.get(3);
			printf("ADD: jumpOffset: %li, dst: %li, op1: %li, op2: %lu\n",
				offset.contents.val, dst.contents.val, op1.contents.val, op2.contents.val
			);
		}
		default: // Unknown instruction
			return false;
			break;
	}
	execStack.clear();
	return true;
}
