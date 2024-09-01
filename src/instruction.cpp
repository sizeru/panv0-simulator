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
u16 cto(const void *insn) {
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
}

Instruction::Instruction() : len(0), op(OP__MAX), data() {}

Instruction::Instruction(u8 *imem, Register &pc)
    : len(parseLen(imem, pc)), op(parseOp(imem, pc)),
      data(parseData(imem, pc)) {}

void Instruction::predictPC(Register& pc) {
	switch (this->op) {
		case OP_JUMP:
			pc.val += this->data.val;
			break;
		case OP_JUMPABS:
			// A more complex deref needs to happen here
			pc.val += this->len;
			break;
		default:
			pc.val += this->len;
			break;
	}
	return;
}

u16 Instruction::parseLen(const u8* imem, const Register& pc) {
	const void* insnbits = reinterpret_cast<const void*>(imem + pc.val);
	u64 h = cto(insnbits) + 1;
	if (h == 1) {
		return 1;
	}
	u32 a = bfe(*(u64 *)insnbits, 0, h);
	u32 b = bfe(*(u64 *)insnbits, h, h);
	return 2 * a + b;
}

u16 Instruction::parseSuffixLen(const u8* imem, const Register& pc) {
	bool lsb = *imem & 1;
	const void* insnbits = reinterpret_cast<const void*>(imem + pc.val);
	return (cto(insnbits) + 1) * (1 + lsb);
}

u16 Instruction::parseOpLen(u16 instrLen) {
	switch (instrLen) {
		case 0: 
			// Instructions of length 0 makes no sense
			return 0;
		case 1:
			return 3;
		case 2: case 3: case 4:
			return instrLen * 2;
		default:
			return 2 * (4 + instrLen / 4);
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
	printf("\tfrom instruction, parsed op: %u (%s)\n", op, opNames[op]);
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
	
	Data data(reinterpret_cast<const void*>(imem + pc.val), suffixLen + opLen, instrLen * 8 - (suffixLen + opLen));
	// printf("\tData was %ub. Extended to %uB. (", instrLen*8-(suffixLen+opLen), data.length);
	// printbits(&data.val, data.length);
	// printf(")\n");
	return data;

	// u64 debugVal = data.val;
	// if (shouldSignExtend) {
	// 	signExtend(reinterpret_cast<u8*>(&debugVal), data.length*8, 64);
	// }
	// printf("\t[ %s %li ]\n", opNames[op], debugVal);
	// return Instruction{op, data, instrLen};
}

// Returns true if the instruction was able to be run. False otherwise.
// Consider this to be the ALU
// TODO: This whole function assumes data fits within 8B
bool Instruction::run(Stack& exec, Stack& prep, Stack& spm, Regfile& regs, u8* mem, u16 wordSize) {
	switch(this->op) {
		case OP_PREP: {
			// NOTE(Nate): Prep instructions will be classified as literals
			return prep.push(this->data, DATA_IS_INTEGER | DATA_IS_LITERAL); // This data length is guaranteed >0.
		}
		case OP_PUSHI: case OP_PUSHS: case OP_PUSHR: {
			Data memVarLen = prep.get(0, 64);
			DataFlags flags = memVarLen.val == 0 ? DATA_IS_LITERAL : DATA_IS_ADDRESS;
			flags |= static_cast<DataFlag>(this->op << 1);
			u32 dataLen;
			if(flags.isAddress() && flags.getLocation() != DATA_IS_SPM) {
				dataLen = memVarLen.val;
			} else {
				dataLen = this->data.len;
			}

			printf("\tPushing %u bit %s value of 0x%lx to stack\n", dataLen, flags.isAddress() ? "address" : "literal", this->data.val);
			bool ok = exec.push(&this->data.val, dataLen, flags);
			if (!ok) {
				return false;
			}
			prep.clear();
			return true;
		}
		case OP_ADD: {
			u64 resLen = this->data.val * 8;
			if (resLen == 0) {
				resLen = exec.getLen(0);
			}
			Data a = exec.get(1, resLen, true);
			Data b = exec.get(2, resLen, true);
			Data shift = exec.get(3, resLen);
			exec.clear();

			u64 result = a.val + (b.val << shift.val);
			printf("\t(%lub ADD) %lx = %lx + (%lx << %lx)\n", resLen, result, a.val, b.val, shift.val);
			return exec.write(0, &result, resLen);
		}
		case OP_JUMPABS: {
			Data condCode = this->data;
			Data target = exec.get(0, wordSize); // where to jump to
			Data op1 = exec.get(1);
			Data op2 = exec.get(2);
			// TODO: Add support for other cond codes
			if (op1.val == op2.val) {
				regs[REG_PC].val = target.val;
			} else {
				regs[REG_PC].val += this->len;
			}
			printf("\t(JUMP.ABS): cond: %lx target: %lx, op1: %li, op2: %lu\n",
				condCode.val, target.val, op1.val, op2.val
			);
			exec.clear();
			return true;
		}
		case OP_JUMP: {
			Data offset = this->data;
			offset.resize(wordSize);
			Data cond = exec.get(0);
			Data op1 = exec.get(1);
			Data op2 = exec.get(2);
			if (op1.val == op2.val) {
				regs[REG_PC].val += offset.val;
			} else {
				regs[REG_PC].val += this->len;
			}
			printf("\tjumpOffset: %li, cond: %lx, op1: %li, op2: %lu\n",
				offset.val, cond.val, op1.val, op2.val
			);
			exec.clear();
			return true;
		}
		default: // Unknown instruction
			return false;
			break;
	}
	return true;
}
