#include "stack.h"

#include <cstring>

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

