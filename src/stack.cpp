#include "data.h"
#include "instruction.h"
#include "panresult.h"
#include "regfile.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdio.h>

// A stack holds a deep copy of any data pushed into it.
Stack::Stack(
	u32 bufferSize, u8* memory, Stack* spmStack, Regfile& regfile, u16 wordSize
) :
		buffer(new u8[bufferSize]), capacity(bufferSize), memory(memory),
		dataCap((bufferSize / 9) * 8), metaCap(bufferSize - dataCap), dataSize(0),
		metaSize(0), metaBuffer((u64*)buffer), dataBuffer(buffer + metaCap),
		spm(spmStack), regs(regfile), wordSize(wordSize) {}

Stack::~Stack() { delete[] buffer; }

// Push a value to the stack
bool Stack::push(const void* data, u32 len, DataFlags dataFlags) {
	// Error checking
	if (8 * this->metaSize + 8 > this->metaCap) {
		return false;
	}
	if ((this->dataSize + (len + 7) / 8) > this->dataCap) {
		return false;
	}

	// Create the metadata
	u64 metadata = ((u64)this->dataSize << 32) | (len << 3) | dataFlags;
	metaBuffer[metaSize++] = metadata;

	// Add the data
	memcpy(dataBuffer + dataSize, data, (len + 7) / 8);
	dataSize += (len + 7) / 8;

	return true;
}

// Push a Data to the stack
bool Stack::push(Data data, DataFlags flags) {
	return this->push(&data.val, data.len, flags);
	// printf("\tPrinting Stack::push data\n");
	// if (data.len > 8) {
	// 	printf("\t\tptr: %p\n", data.contents.ptr);
	// 	printf("\t\tlength: %d\n", data.len);
	// 	printf("\t\tflags: %i\n", static_cast<u32>(data.flags));
	// 	return this->push(data.contents.ptr, data.len, data.flags);
	// } else {
	// 	printf("\t\tval: %ld\n", data.val);
	// 	printf("\t\tlength: %d\n", data.len);
	// 	printf("\t\tflags: %i\n", static_cast<u32>(data.flags));
	// 	return this->push(&data.val, data.len, data.flags); // This was previously
	// flipped. Bug
	// }
}

// Retrieve the specified value from the stack, and expand/truncate it
// to the specified size (in bits). The flags determine where the data
// will be retrieved from, and whether the data will be sign extended or not.
Data Stack::get(u32 index, u8 size, bool isSigned) {
	if (index >= this->metaSize) {
		printf("\tStack::get(%u) oob. Returned 0\n", index);
		return Data();
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of
	                                        // stack is intended
	DataFlags flags = metadata & 7;

	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	uintptr_t dataBufferOffset = metadata >> 32;
	void* dataBufferPtr = dataBuffer + dataBufferOffset;

	if (size == 0) {
		size = lenBytes * 8 + lenBits;
	}

	printf(
		"\tStack::get(%u) -> %ub of data beginning from dataBufferOffset: "
		"0x%lx. Flags: ",
		index,
		lenBits,
		dataBufferOffset
	);
	printbits(&flags, 1);
	printf("\n");
	// The raw data from the instruction is already in the stack. The job
	// of get is to determine if/how to dereference that data
	if (lenBytes > sizeof(size_t)) {
		printf(
			"Instruction lengths longer than %lu not supported in this version\n",
			sizeof(size_t)
		);
		exit(4);
	}

	Data retrievedData;
	switch (flags.getLocation()) {
	case DATA_IS_INTEGER: {
		if (flags.isAddress()) {
			// The size of an address is fixed on this machine.
			// The length of the dereferenced data is variable
			u64 stackData = 0;
			memcpy(&stackData, dataBufferPtr, this->wordSize / 8);
			void* addr = memory + stackData;
			memcpy(&retrievedData.val, addr, lenBytes);
		} else {
			// The length of the literal stack data is variable
			memcpy(&retrievedData.val, dataBufferPtr, lenBytes);
		}
		retrievedData.len = lenBits;
		break;
	}
	case DATA_IS_REG: {
		u64 regIndex = 0;
		// This machine has < 8 registers. This fits within 1 byte.
		// NOTE(Nate): This is not forward-compatible
		memcpy(&regIndex, dataBufferPtr, 1);
		Register& reg = this->regs[regIndex];
		if (flags.isAddress()) {
			void* addr = memory + reg.val;
			memcpy(&retrievedData.val, addr, lenBytes);
		} else {
			retrievedData.val = reg.val;
		}
		retrievedData.len = lenBytes * 8 + lenBits;
		break;
	}
	case DATA_IS_SPM: {
		// The spm stack stores the size of the data inside of it. Should
		// the spm stack store an address, the address must be of a known
		// size, and thus, the spm stack stores the length of the address
		// inside of it. NOTE(Nate): Is this forward-compatible?

		// Get SPM index
		i32 spmIndex = 0;
		memcpy(&spmIndex, dataBufferPtr, lenBytes);
		signExtend(reinterpret_cast<u8*>(&spmIndex), lenBits, sizeof(i32) * 8);
		// Convert relative spmIndex to absolute spmIndex
		spmIndex += this->regs[REG_ANC].val;
		printf(
			"\tSPM%li -> SPM.ABS%i\n", spmIndex - this->regs[REG_ANC].val, spmIndex
		);
		if (flags.isAddress()) {
			u64 wordSize = this->regs[REG_SP].size;
			u64 addrVal = spm->get(spmIndex, wordSize).val;
			void* addr = memory + addrVal;
			memcpy(&retrievedData.val, addr, lenBytes);
			retrievedData.len = size;
			printf(
				"\tRead (ABS) SPM[%u]: addr %lx (val: %lu)\n",
				spmIndex,
				addrVal,
				retrievedData.val
			);
		} else {
			retrievedData = spm->get(spmIndex, size);
		}
		break;
	}
	default: panExit(PAN_ERR_DATA_MISSING_LOCATION);
	}

	retrievedData.resize(size, isSigned);
	// printf("\tData: %lx\n", d.val);
	return retrievedData;
}

// Writes to a destination indicated on the stack.
bool Stack::write(u32 index, void* val, u32 storeLen) {
	if (index >= this->metaSize) {
		return true;
	}

	// Clean up value to be stored (zero out top byte)
	// u32 storeOffset = storeLen % 8;
	u32 storeBytes = (storeLen + 7) / 8;
	// u8* msb = reinterpret_cast<u8*>(val) + storeByte;
	// *msb &= (1 << storeOffset) - 1;

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of
	                                        // stack is intended
	DataFlags flags = metadata & 7;
	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	void* dataBufferOffset = reinterpret_cast<void*>(((metadata >> 32) + 7) / 8);
	void* dataBufferPtr =
		dataBuffer + reinterpret_cast<uintptr_t>(dataBufferOffset);

	switch (flags.getLocation()) {
	case DATA_IS_INTEGER: {
		u64 ptr = 0;
		if (flags.isAddress()) {
			// The size of an address is fixed on this machine.
			// The length of the dereferenced data is variable
			u32 minBytes = std::min(lenBytes, storeBytes);
			memcpy(&ptr, dataBufferPtr, this->wordSize / 8);
			memcpy(memory + ptr, val, minBytes);
			printf("\twrote %uB to int literal address: %lx\n", minBytes, ptr);
		}
		// Writing to a literal integer is throwing away a value.
		// NOTE(Nate): This is potentially wasteful. On one hand, throwing
		// away a value allows us to implement a NOP. On another hand, we
		// could be doing something instead of writing to nothing. Another
		// idea is that writing to a literal integer means writing to that
		// address, while writing to a an address means dereferencing the
		// value at a particular place in memory and writing there. This
		// is potentially more useful, as it allows for the C line: `int*
		// x = *(int*)(0xdeadbeef)` without the need for transient values.
		// ANOTHER possible option is that writing to a literal writes to
		// a literal value on this stack.
		break;
	}
	case DATA_IS_REG: {
		u64 regIndex = 0;
		// This machine has < 8 registers. This fits within 1 byte.
		// NOTE(Nate): This is not forward-compatible
		memcpy(&regIndex, dataBufferPtr, 1);
		Register& reg = this->regs[regIndex];
		if (flags.isAddress()) {
			u32 minBytes = std::min(lenBytes, storeBytes);
			// i64 offset = reg.val;
			// signExtend(reinterpret_cast<u8*>(&offset), reg.size, 64);
			memcpy(memory + reg.val, val, minBytes);
			printf(
				"\twrote %uB to reg[%lu] address: %lx\n", minBytes, regIndex, reg.val
			);
		} else {
			// Writing to a register overwrites the whole register
			reg.val = 0;
			u32 minBytes = std::min((u32)reg.size + 7 / 8, storeBytes);
			memcpy(&reg.val, val, minBytes);
			printf("\twrote %uB to reg[%lu]\n", minBytes, regIndex);
		}
		break;
	}
	case DATA_IS_SPM: {
		// A value on the spm stack stores its size
		u32 spmIndex = 0; // absolute index
		memcpy(&spmIndex, dataBufferPtr, lenBytes);
		spmIndex += this->regs[REG_ANC].val;
		if (spmIndex >= spm->metaSize) {
			assert(spmIndex == spm->metaSize); // Skipping not allowed
			assert(!flags.isAddress());        // Cannot deref without an address
			printf(
				"\tSPM: pushing spm[%u] as '%s' onto spm stack\n",
				spmIndex,
				flags.isAddress() ? "address" : "literal"
			);
			if (storeLen == 0) { // Must be an address
				u64 wordSize = this->regs[REG_SP].size;
				storeLen = wordSize;
			}
			spm->push(val, storeLen, DATA_IS_INTEGER);
		} else {
			if (flags.isAddress()) {
				Data spmData = spm->get(spmIndex, this->wordSize);
				memcpy(memory + spmData.val, val, storeBytes);
				printf(
					"\twrote %uB to spm[%u] address: %lx\n",
					storeBytes,
					spmIndex,
					spmData.val
				);
			} else {
				// Overwriting the contents of a spm value. Note that a spm
				// value can never be grown once set
				// NOTE(Nate): This if statement assumes that the compiler /
				// programmer will always initialize new spm registers in order.
				spm->replace(spmIndex, val, storeLen);
				printf("\twrote %ub to spm[%u]\n", storeLen, spmIndex);
			}
		}
		break;
	}
	default: panExit(PAN_ERR_DATA_MISSING_LOCATION);
	}

	printf("\twrote %ub value '%lx'.\n", storeLen, *(u64*)(val));
	return true;
}

// Replace all or part of a value on the stack with another.
void Stack::replace(u32 index, void* val, u32 len) {
	if (index >= this->metaSize) {
		return;
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of
	                                        // stack is intended
	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	void* dataBufferPtr = this->dataBuffer + (metadata >> 32);

	u64 minLen = std::min(lenBytes, len);
	memcpy(dataBufferPtr, val, minLen);
	// memset(reinterpret_cast<void*>((uintptr_t)dataBufferPtr + minLen), 0,
	// lenBytes - minLen);
}

// Return a pointer to the data at this index. Useful for reading or writing
// literals.
void* Stack::getData(u32 index) {
	if (index >= this->metaSize) {
		return nullptr;
	}
	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of
	                                        // stack is intended
	return reinterpret_cast<void*>(this->dataBuffer + (metadata >> 32));
}

void Stack::clear() {
	this->dataSize = 0;
	this->metaSize = 0;
}

DataFlags Stack::getFlags(u32 index) {
	if (index >= this->metaSize) {
		return 0;
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of
	                                        // stack is intended
	return metadata & 7;
}

u32 Stack::getLen(u32 index) {
	if (index >= this->metaSize) {
		return 0;
	}

	u64 metadata = this->metaBuffer[index];
	DataFlags flags = metadata & 7;
	u64 dataSize = (metadata & ((1UL << 32) - 1)) >> 3;
	u64 dataOffset = metadata >> 32;
	u8* data = this->dataBuffer + dataOffset;
	u32 lenBytes = (dataSize + 7) / 8;

	u32 len;
	switch (flags.getLocation()) {
	case DATA_IS_INTEGER: {
		len = dataSize;
		break;
	}
	case DATA_IS_SPM: {
		if (flags.isAddress()) {
			len = dataSize;
		} else {
			// The spm stack stores its own information
			u64 spmIndex = 0;
			memcpy(&spmIndex, data, lenBytes);
			len = this->spm->getLen(index);
		}
		break;
	}
	case DATA_IS_REG: {
		u64 regIndex = 0;
		memcpy(&regIndex, data, lenBytes);
		len = this->regs[regIndex].size;
		break;
	}
	default: {
		panExit(PAN_ERR_DATA_MISSING_LOCATION);
		break;
	}
	}

	return len;
}

void Stack::printStatus() {
	printf("Stack Info:\n");
	for (int i = 0; i < this->metaSize; i += 1) {
		Data d = this->get(i);
		printf("\t%i: len: %u, val: %lx\n", i, d.len, d.val);
	}
}
