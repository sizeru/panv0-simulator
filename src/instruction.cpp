#include <cstring>
#include <stdio.h>
#include "instruction.h"
#include "data.h"

// For debugging purposes
void printbits(void *insn, unsigned char len) {
	insn = (u8 *)insn + len-1;
	for (int i = len-1; i >= 0; i--) {
		u8 byte = *(u8 *)insn;
		for (int i = 7; i >= 4; i--) putchar('0' + ((byte >> i) & 1));
		printf("_");
		for (int i = 3; i >= 0; i--) putchar('0' + ((byte >> i) & 1));
		insn = (u8 *)insn - 1;
		if (i-1 >= 0) printf(" ");
	}
}

// Count trailing ones of an arbitrary data type
u16 cto(void *insn) {
	u16 ones_count = 0;
	while (1) {
		u8 byte = *(u8 *)insn;
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
		case 2: case 3: case 4:
			opLen = instrLen * 2;
			break;
		default:
			opLen = 2 * (4 + instrLen / 4);
			break;
	}

	if (opLen > 8) {
		// Unsupported
		op = OP__MAX;
		return;
	}

	op = (Operation)bfe(*(u64 *)instr, suffixLen, opLen);
	return;
}

// Reads in the instruction bits and retrieves the opcode, data, and
// dataLength, and instruction length from an instructions bits.
Instruction getInstr(void *instrPos) {
	u32 instrLen, h;
	getInstrLen(instrPos, instrLen, h);
	u32 suffixLen = h == 1 ? h : 2 * h; // If h == 1 then special case
	printf("\tRead %u B instruction (", instrLen);
	printbits(instrPos, instrLen);
	printf(")\n");

	Operation op;
	u32 opLen;
	getOp(op, opLen, instrPos, instrLen, suffixLen);
	printf("\tOp: %u (%s)\n", op, opNames[op]);

	// TODO(Nate): We should not be sign extending PUSH.I address
	// instructions. This likely means that sign extension should only
	// occur upon executing a non-prep instruction. This is passable for
	// now, since we never push addresses.
	bool shouldSignExtend = op == OP_PUSHI | op == OP_PUSHS;
	Data data(instrPos, suffixLen + opLen, instrLen * 8 - (suffixLen + opLen), shouldSignExtend);
	printf("\tData was %ub. Extended to %uB. (", instrLen*8-(suffixLen+opLen), data.length);
	printbits(&data.contents.val, data.length);
	printf(")\n");

	u64 debugVal = data.contents.val;
	if (shouldSignExtend) {
		signExtend(reinterpret_cast<u8*>(&debugVal), data.length*8, 64);
	}
	printf("\t[ %s %li ]\n", opNames[op], debugVal);
	return Instruction{op, data, instrLen};
}

// Returns true if the instruction was able to be run. False otherwise.
// Consider this to be the ALU
// TODO: This whole function assumes data fits within 8B
bool runInstr(const Instruction& instr, Stack& prepStack, Stack& execStack, Stack& spmStack, u8 memory[], Regfile& regs) {
	switch(instr.op) {
		case OP_PREP: {
			// NOTE(Nate): Prep instructions will be classified as literals
			return prepStack.push((void*)&instr.data.contents.val, instr.data.length, DATA_IS_INTEGER | DATA_IS_LITERAL); // This data length is guaranteed >0.
		}
		case OP_PUSHI: {
			size_t dataSize = prepStack.get(0).contents.val;
			DataFlags flags = dataSize == 0 ? DATA_IS_LITERAL : DATA_IS_ADDRESS;
			flags |= DATA_IS_INTEGER;
			u32 dataLength = flags.isAddress() ? dataSize : instr.data.length;
			i64 debugVal = instr.data.contents.val;
			signExtend(reinterpret_cast<u8*>(&debugVal), instr.data.length*8, 64);
			printf("\tPushing %uB %s integer of value %li to stack\n", dataLength, flags.isAddress() ? "address" : "literal", debugVal);
			bool ok = execStack.push((void*)&instr.data.contents.val, dataLength, flags);
			if (!ok) {
				return false;
			}
			prepStack.clear();
			return true;
		}
		case OP_PUSHS: {
			size_t dataSize = prepStack.get(0).contents.val;
			DataFlags flags = dataSize == 0 ? DATA_IS_LITERAL : DATA_IS_ADDRESS;
			flags |= DATA_IS_SPM;
			u32 dataLength = flags.isAddress() ? dataSize : instr.data.length;
			bool ok = execStack.push((void*)&instr.data.contents.val, dataLength, flags);
			if (!ok) {
				return false;
			}
			prepStack.clear();
			return true;
		}
		case OP_PUSHR: {
			size_t dataSize = prepStack.get(0).contents.val;
			DataFlags flags = dataSize == 0 ? DATA_IS_LITERAL : DATA_IS_ADDRESS;
			flags |= DATA_IS_REG;
			u32 dataLength = flags.isAddress() ? dataSize : instr.data.length;
			bool ok = execStack.push((void*)&instr.data.contents.val, dataLength, flags);
			if (!ok) {
				return false;
			}
			prepStack.clear();
			return true;
		}
		case OP_ADD: {
			Data resultLen = instr.data;
			Data dst = execStack.get(0);
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			Data shift = execStack.get(3);
			execStack.clear();

			u64 valA = op1.contents.val;
			u64 valB = op2.contents.val;
			u64 valShift = shift.contents.val;

			u64 result = valA + (valB << valShift);
			printf("\t(ADD) %lx = %lx + (%lx << %lx)\n", result, valA, valB, valShift);
			switch(dst.flags.getLocation()) {
				case DATA_IS_INTEGER: {
					// Integers address destinations must have number length. Literal destinations do nothing.
					if (dst.flags.isAddress()) {
						memcpy((void*)(memory + dst.contents.val), &result, dst.length);
					}
				}
				case DATA_IS_SPM: {
					if (dst.flags.isAddress()) {
						memcpy((void*)(memory + spmStack.get(dst.contents.val).contents.val), &result, dst.length);
					} else {
						// TODO: Need to convert relative spm number to absolute spm number
						void* spmData = spmStack.getData(dst.contents.val);
						if (!spmData) {
							spmStack.push(0, dst.length, DATA_IS_INTEGER | DATA_IS_LITERAL);
							spmData = spmStack.getData(dst.contents.val);
						}
						memcpy(spmData, &result, dst.length);
					}
				}
				case DATA_IS_REG: {
					if (dst.flags.isAddress()) {
						memcpy((void*)(memory + regs[dst.contents.val].val), &result, dst.length);
					} else {
						result &= (1UL << regs[dst.contents.val].size) - 1;
						memcpy(&regs[dst.contents.val].val, &result, dst.length);
					}
				}
			}
			printf("\t Result: %lu\n", result);
			return true;
		}
		case OP_JUMPABS: {
			Data condCode = instr.data;
			Data target = execStack.get(0); // where to jump to
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			printf("\tADD: cond: %lu, dst: %li, op1: %li, op2: %lu\n",
				condCode.contents.val, target.contents.val, op1.contents.val, op2.contents.val
			);
			execStack.clear();
			return true;
		}
		case OP_JUMP: {
			Data offset = instr.data;
			Data dst = execStack.get(0);
			Data op1 = execStack.get(1);
			Data op2 = execStack.get(2);
			Data shift = execStack.get(3);
			printf("\tADD: jumpOffset: %li, dst: %li, op1: %li, op2: %lu\n",
				offset.contents.val, dst.contents.val, op1.contents.val, op2.contents.val
			);
			execStack.clear();
			return true;
		}
		default: // Unknown instruction
			return false;
			break;
	}
	return true;
}
