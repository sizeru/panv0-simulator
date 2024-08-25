#include "stack.h"
#include "instruction.h" 

#include <cstring>
#include <stdio.h>

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
    printf("\tPrinting Stack::push data\n");
	if (data.length > 8) {
        printf("\t\tptr: %p\n", data.contents.ptr);
        printf("\t\tlength: %d\n", data.length);
        printf("\t\tflags: %d\n", data.flags);
		return this->push(data.contents.ptr, data.length, data.flags);
	} else {
        printf("\t\tval: %ld\n", data.contents.val);
        printf("\t\tlength: %d\n", data.length);
        printf("\t\tflags: %d\n", data.flags);
		return this->push(&data.contents.val, data.length, data.flags); // This was previously flipped. Bug
	}
}

// Return the data on the stack at a particular point
Data Stack::get(u32 index) {
	if (index >= this->metaSize) {
		return Data();
	}

	u64 metadata = this->metaBuffer[index]; // TODO CHECK THIS: should this be `this->metaSize-index`. indexing from front instead of from back, not sure if this is intended
	u32 length = (metadata & ((1UL << 32) - 1)) >> 3;
	u32 dataPtr = (metadata >> 32);
	bool isAddress = metadata & 1;

    printf("\tPrinting Stack::get Metadata at index: %d\n\t", index);
    printbits(this->metaBuffer + index, 8);

	Data d;
	d.length = length;

    printf("\t\tlength: %d\n", length);
	if (length > sizeof(size_t)) {
		d.contents.ptr = isAddress ? this->memory + dataPtr : dataBuffer + dataPtr; // TODO: Need to update this, read below
	} else {
		// size_t val = isAddress ? *(size_t*)(memory+(size_t)data) : (size_t)data;  // TODO: Data is currently metadata address in databuffer. it is never the raw data so this line is wrong. Below is what I replaced it with
		size_t val = isAddress ? *(size_t*)(memory+dataPtr) :  dataBuffer[dataPtr]; // update the first one?
		val &= (1UL << (length*8)) - 1;
		d.contents.val = val;
        printf("\t\tval: %ld\n", val);
	}
	return d;
}

void Stack::clear() {
	this->dataSize = 0;
	this->metaSize = 0;
}

