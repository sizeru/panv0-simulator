#include <cstring>
#include <stdio.h>
#include "data.h"
#include "instruction.h" 
#include "panresult.h"

// A stack holds a deep copy of any data pushed into it. By default, a
// stack is able to 


Stack::Stack(u32 bufferSize, u8* memory, Stack* spmStack, Regfile& regfile, u16 wordSize)
	: buffer(new u8[bufferSize]), capacity(bufferSize), memory(memory),
	  dataCap((bufferSize / 9) * 8), metaCap((bufferSize - dataCap)/8), dataSize(0),
	  metaSize(0), metaBuffer((u64*)buffer), dataBuffer(buffer + metaCap),
		spm(spmStack), regs(regfile), wordSize(wordSize) {}

Stack::~Stack() {
	delete[] buffer;
}

// Push a value to the stack
bool Stack::push(void* data, u32 len, DataFlags dataFlags) {
	// Error checking  
	if (8*this->metaSize + 8 > this->metaCap) {
		return false;
	}
	if ((this->dataSize + (len+7)/8) > this->dataCap) {
		return false;
	}

	// Create the metadata
	u64 metadata = ((u64)this->dataSize << 32) | (len << 3) | dataFlags;
	metaBuffer[metaSize++] = metadata;

	// Add the data
	memcpy(dataBuffer + dataSize, data, (len + 7) / 8);
	dataSize += (len+7)/8;
	
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
	// 	return this->push(&data.val, data.len, data.flags); // This was previously flipped. Bug
	// }
}

// Retrieve the specified value from the stack, and expand/truncate it
// to the specified size (in bits). The flags determine where the data
// will be retrieved from, and whether the data will be sign extended or not.
Data Stack::get(u32 index, u8 size, bool isSigned) {
	if (index >= this->metaSize) {
		return Data();
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
	DataFlags flags = metadata & 7;

	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	void* dataBufferOffset = reinterpret_cast<void*>(metadata >> 32);
	void* dataBufferPtr = dataBuffer + reinterpret_cast<uintptr_t>(dataBufferOffset);

	if (size == 0) {
		size = lenBits;
	}

	printf("\tStack::get(%u) -> %ub of data beginning from dataBufferOffset: %p. Flags: ", index, lenBits, dataBufferOffset);
	printbits(&flags, 1);
	printf("\n");
	// The raw data from the instruction is already in the stack. The job
	// of get is to determine if/how to dereference that data
	if (lenBytes > sizeof(size_t)) {
		printf("Instruction lengths longer than %lu not supported in this version\n", sizeof(size_t));
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
			retrievedData.len = lenBits;
			break;
		}
		case DATA_IS_SPM: {
			// The spm stack stores the size of the data inside of it. Should
			// the spm stack store an address, the address must be of a known
			// size, and thus, the spm stack stores the length of the address
			// inside of it. NOTE(Nate): Is this forward-compatible?
			u32 spmIndex = 0;
			memcpy(&spmIndex, dataBufferPtr, lenBytes);
			retrievedData = spm->get(spmIndex, size);
			break;
		}
		default: {
			panExit(PAN_ERR_DATA_MISSING_LOCATION);
		}
	}

	retrievedData.resize(size, isSigned);
	// printf("\tData: %lx\n", d.val);
	return retrievedData;
}

bool Stack::write(u32 index, void* val, u32 len) {
	if (index >= this->metaSize) {
		return true;
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
	DataFlags flags = metadata & 7;
	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	void* dataBufferOffset = reinterpret_cast<void*>(((metadata >> 32) + 7) / 8);
	void* dataBufferPtr = dataBuffer + reinterpret_cast<uintptr_t>(dataBufferOffset);

	switch (flags.getLocation()) {
		case DATA_IS_INTEGER: {
			u64 ptr = 0;
			if (flags.isAddress()) {
				// The size of an address is fixed on this machine.
				// The length of the dereferenced data is variable
				memcpy(&ptr, dataBufferPtr, this->wordSize / 8);
				memcpy(memory + ptr, val, lenBytes);
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
				memcpy(memory + reg.val, val, lenBytes);
			} else {
				// Writing to a register overwrites the whole register
				reg.val = 0;
				memcpy(&reg.val, val, lenBytes);
			}
			break;
		}
		case DATA_IS_SPM: {
			// The spm stack stores the size of the data inside of it.
			// NOTE(Nate): This is not forward-compatible
			u32 spmIndex = 0;
			memcpy(&spmIndex, dataBufferPtr, lenBytes);
			if (flags.isAddress()) {
				memcpy(memory + spm->get(spmIndex, this->wordSize).val, val, lenBytes);
			} else {
				// Overwriting the contents of a spm value. Note that a spm
				// value can never be grown
				spm->replace(spmIndex, val, lenBits);
			}
			break;
		}
		default: {
			panExit(PAN_ERR_DATA_MISSING_LOCATION);
		}
	}
	return true;
}

// Replace a value on the stack with another.
void Stack::replace(u32 index, void* val, u32 len) {
	if (index >= this->metaSize) {
		return;
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
	u32 lenBits = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 lenBytes = (lenBits + 7) / 8;
	void* dataBufferPtr = reinterpret_cast<void*>(metadata >> 32);

	u64 minLen = lenBytes < len ? lenBytes : len;
	memcpy(dataBufferPtr, val, minLen);
	memset(reinterpret_cast<void*>((uintptr_t)dataBufferPtr + minLen), 0, lenBytes - minLen);

}

// Return a pointer to the data at this index. Useful for reading or writing literals.
void* Stack::getData(u32 index) {
	if (index >= this->metaSize) {
		return nullptr;
	}
	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
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

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
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
			// The spm stack stores its own information
			u64 spmIndex = 0;
			memcpy(&spmIndex, data, lenBytes);
			len = this->spm->getLen(index);
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