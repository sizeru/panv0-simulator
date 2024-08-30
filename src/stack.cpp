#include "data.h"
#include "instruction.h" 
#include <cstring>
#include <stdio.h>

// A stack holds a deep copy of any data pushed into it. By default, a
// stack is able to 


Stack::Stack(u32 bufferSize, u8* memory, Stack* spmStack, Regfile& regfile)
	: buffer(new u8[bufferSize]), capacity(bufferSize), memory(memory),
	  dataCap((bufferSize / 9) * 8), metaCap((bufferSize - dataCap)/8), dataSize(0),
	  metaSize(0), metaBuffer((u64*)buffer), dataBuffer(buffer + metaCap),
		spm(spmStack), regs(regfile) {}

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
	printf("\tPrinting Stack::push data\n");
	if (data.length > 8) {
		printf("\t\tptr: %p\n", data.contents.ptr);
		printf("\t\tlength: %d\n", data.length);
		printf("\t\tflags: %i\n", static_cast<u32>(data.flags));
		return this->push(data.contents.ptr, data.length, data.flags);
	} else {
		printf("\t\tval: %ld\n", data.contents.val);
		printf("\t\tlength: %d\n", data.length);
		printf("\t\tflags: %i\n", static_cast<u32>(data.flags));
		return this->push(&data.contents.val, data.length, data.flags); // This was previously flipped. Bug
	}
}

// Retrieved the data specified on the stack. Different flags will
// retrieve the data from different locations.
Data Stack::get(u32 index) {
	if (index >= this->metaSize) {
		return Data();
	}

	u64 metadata = this->metaBuffer[index]; // Absolute indexing from base of stack is intended
	u32 length = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 dataBufferPtr = (metadata >> 32);
	DataFlags flags = metadata & 7;
	

	printf("\tStack::get(%u) -> %uB of data beginning from dataBuffer[%u]. Flags: ", index, length, dataBufferPtr);
	printbits(&flags, 1);
	printf("\n");

	Data d;
	d.length = length;
	d.flags = flags;

	// The raw data from the instruction is already in the stack. The job
	// of get is to determine if/how to dereference that data
	if (length > sizeof(size_t)) {
		printf("Instruction lengths longer than %lu not supported in this version\n", sizeof(size_t));
		exit(4);
	} else {
		size_t data = 0;
		memcpy(&data, dataBuffer + dataBufferPtr, length);

		switch (flags.getLocation()) {
			case DATA_IS_INTEGER:
				d.contents.val = flags.isAddress() ? *(size_t*)(this->memory+data) : data;
				d.contents.val &= (1UL << length*8) - 1;
				break;
			case DATA_IS_REG:
				d.contents.val = flags.isAddress() ? *(size_t*)(this->memory + this->regs[data].val) : this->regs[data].val;
				d.contents.val &= (1UL << this->regs[data].size*8) - 1;
				d.length = this->regs[data].size;
				break;
			case DATA_IS_SPM: {
				Data spmData = spm->get(data);
				size_t spmVal = spmData.contents.val & ((1UL << spmData.length) - 1);
				d.contents.val = flags.isAddress() ? *(size_t*)(this->memory+spmVal) : spmData.contents.val;
				d.contents.val &= (1UL << d.length*8) - 1;
				d.length = this->regs[data].size;
				break;
			}
			default:
				break;
		}
			printf("\tData: %lx\n", d.contents.val);
	}
	return d;
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

