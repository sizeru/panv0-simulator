#ifndef STACK_H
#define STACK_H

#include "shorttypes.h"
#include "data.h"

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

#endif // STACK_H
