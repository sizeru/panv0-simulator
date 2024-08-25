/*
 * NOTE: This currently only supports instructions up to 8 bytes
 */
#include "shorttypes.h"
#include <cstdint>
#include <cstring>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

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

enum DataFlag {
	// The following flags are mutually exclusive
	DATA_IS_LITERAL = 0x0,
	DATA_IS_ADDRESS = 0x1,
	// The previous flags are mutually exclusive
	// The following flags are mutually exclusive
	DATA_IS_INTEGER = 0x2,
	DATA_IS_SPM = 0x4,
	DATA_IS_REG = 0x6,
	// The previous flags are mutually exclusive
};

typedef u32 DataFlags;

struct Data {
	u32 length;
	union {
		size_t val;
		u8* ptr;
	} contents;
	DataFlags flags;

	Data();
	Data(void* instr, u32 suffixLen, u32 opLen, u32 instrLen);
	~Data();
};


class Stack {
	// The necessary
	u8* const buffer;
	const u32 capacity;

	// The derived
	u8* memory;
	const u32 dataCap;
	const u32 metaCap; // 8B units
	u32 dataSize;
	u32 metaSize; // 8B units

	u64* const metaBuffer; // Each metadata is 64b long
	u8* const dataBuffer;

public:
	Stack(u32 bufferSize, u8* memory);
	~Stack();

	bool push(Data data);
	bool push(void* data, u32 length, DataFlags flags);
	void clear();
	Data get(u32 index);
};

Stack::Stack(u32 bufferSize, u8* memory)
	: buffer(new u8[bufferSize]), capacity(bufferSize), memory(memory),
	  dataCap((bufferSize / 9) * 8), metaCap((bufferSize - dataCap)/8), dataSize(0),
	  metaSize(0), metaBuffer((u64*)buffer), dataBuffer(buffer + metaCap) {}

Stack::~Stack() {
	delete[] buffer;
}

// Push a value to the stack
bool Stack::push(void* data, u32 length, DataFlags dataFlags) {
	// Error checking  
	if (8*this->metaSize + 8 > this->metaCap) {
		return false;
	}
	if ((this->dataSize + length) > this->dataCap) {
		return false;
	}

	// Create the metadata
	u64 metadata = ((u64)this->dataSize << 32) | (length << 3) | dataFlags;
	metaBuffer[metaSize++] = metadata;

	// Add the data
	memcpy(dataBuffer + dataSize, data, length);
	dataSize += length;
	
	return true;
}

// Push a Data to the stack
bool Stack::push(Data data) {
	if (data.length > 8) {
		return this->push(&data.contents.val, data.length, data.flags);
	} else {
		return this->push(data.contents.ptr, data.length, data.flags);
	}
}

// Return the data on the stack at a particular point
Data Stack::get(u32 index) {
	if (index >= this->metaSize) {
		return Data();
	}

	u64 metadata = this->metaBuffer[index];
	u32 length = (metadata & ((1UL << 32) - 1)) >> 3;
	u8* data = (u8*)(metadata >> 32);
	bool isAddress = metadata & 1;

	Data d;
	d.length = length;

	if (length > sizeof(size_t)) {
		d.contents.ptr = isAddress ? this->memory + (size_t)data : data;
	} else {
		size_t val = isAddress ? *(size_t*)(memory+(size_t)data) : (size_t)data;
		val &= (1UL << (length*8)) - 1;
		d.contents.val = val;
	}
	return d;
}

void Stack::clear() {
	this->dataSize = 0;
	this->metaSize = 0;
}

struct Instruction {
	Operation op;
	u32 len;
	Data data;
};

struct ExecStackLine {
	Data data;
	bool isAddress;
};

// Count trailing ones of an arbitrary data type
u16 cto(void *insn) {
	u16 ones_count = 0;
	while (1) {
		u8 byte = *(u8 *)insn;
		for (int i = 0; i < 8; i++) {
			if (!(byte & (1 << i))) {
				return ones_count;
			}
		}
		insn = (char *)insn + 1;
	}
}

// Bit feild extract
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
		case 3:
		case 4:
			opLen = 2 * instrLen;
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

Data::Data() : length(0), flags(0) {}

// Extracts the data (excluding flags) from an instruction
Data::Data(void *instr, u32 suffixLen, u32 opLen, u32 instrLen) {
	u32 dataLen = (instrLen * 8) - opLen - suffixLen; // length in bits
	// Ensure dataLen is always a multiple of 2 and is stored in bytes
	u32 rounded = dataLen;
	rounded |= rounded >> 1;
	rounded |= rounded >> 2;
	rounded |= rounded >> 4;
	rounded |= rounded >> 8;
	rounded |= rounded >> 16;
	rounded += 1;
	rounded >>= 3;
	this->length = rounded;
	u8* data;
	if (rounded > 8) {
		this->contents.ptr = new u8[(8*rounded + 7) / 8]();
		data = (u8*)this->contents.ptr;
	} else {
		this->contents.val = 0;
		data = (u8*)&this->contents.val;
	}

	// Copy data
	dataLen = (dataLen+7) / 8;
	u8* dataPos = (u8*)instr + (suffixLen+opLen)/8;
	u8 dataOffset = (suffixLen + opLen) % 8;

	// TODO: Segfault here
	data[0] = dataPos[0] >> dataOffset;

	for (int i = 1; i < (dataLen+7)/8; i++) {
		data[i-1] |= dataPos[i] >> (8-dataOffset);
		data[i] |= dataPos[i] >> dataOffset;
	}
	return;
}

Data::~Data() {
	if (this->length > sizeof(size_t)) {
		delete[] this->contents.ptr;
	}
}

Instruction getInstr(void *instrPos) {
	u32 instrLen, h;
	getInstrLen(instrPos, instrLen, h);
	u32 suffixLen = 2 * h;
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
	printf("Instr: op: %u, len: %u, data: %lu\n", op, instrLen, data.contents.val);
	return Instruction{op, instrLen, data};
}

// Returns true if the instruction was able to be run. False otherwise.
bool runInstr(const Instruction& instr, Stack& execStack, Stack& spmStack, u8 const memory[]) {
	if (instr.op < 6) {
		return execStack.push(instr.data);
	}
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

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		puts("./panv0 [-f <inputfile>] [-b <bufferSize>] "
				 "[-e <execution-stack-buffer-size>] [-s <spm-reg-buffer-size>] "
				 "[-c <cache-buffer-size>]");
		return 1;
	}

	char *filename = NULL;
	size_t bufferSize = 4096;
	size_t execStackBufferSize = 4096;
	size_t spmRegBufferSize = 4096;
	size_t cacheBufferSize = 4096;
	// Update defaults with user passed args
	int flag;
	while ((flag = getopt(argc, argv, "f:b:e:s:c:")) != -1) {
		switch (flag) {
		case 'f':
			filename = optarg;
			break;
		case 'b':
			bufferSize = atol(optarg);
			break;
		case 'e':
			execStackBufferSize = atol(optarg);
			break;
		case 's':
			spmRegBufferSize = atol(optarg);
			break;
		case 'c':
			cacheBufferSize = atol(optarg);
			break;
		}
	}

	// Create stacks
	u8 memory[1024*8];
	Stack spmStack(spmRegBufferSize, memory);
	Stack execStack(execStackBufferSize, memory);

	// Allocate buffer for instructions
	unsigned char buffer[bufferSize];
	size_t cursor = 0; // position in buffer
	size_t bytes_read = 0;

	// Load file into buffer
	FILE *file = fopen(filename, "r");
	if (file == nullptr) {
		printf("File: '%s' does not exist\n", filename);
		return 2; 
	}

	// Stream the file into the buffer
	while ((bytes_read = fread(buffer, 1, bufferSize, file)) != 0) {
		printf("Read %lu bytes into buffer\n", bytes_read);
		while (cursor < bytes_read) {
			// Get different instruction fields
			u8* instrPos = buffer + cursor;
			// printf("Read Instr with starting byte: 0x%02x\n", *instrPos);
			Instruction instr = getInstr(instrPos);
			runInstr(instr, execStack, spmStack, memory);
			cursor += instr.len;
		}
	}
}
