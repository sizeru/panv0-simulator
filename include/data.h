#ifndef DATA_H
#define DATA_H

#include "regfile.h"
#include "shorttypes.h"
#include <stdlib.h>

class Stack;

enum DataFlag {
	// The following flags (address) are mutually exclusive
	DATA_IS_LITERAL = 0x0,
	DATA_IS_ADDRESS = 0x1,
	// The previous flags (address) are mutually exclusive
	// The following flags (locations) are mutually exclusive
	DATA_IS_INTEGER = 0x2,
	DATA_IS_SPM = 0x4,
	DATA_IS_REG = 0x6,
	// The previous flags (locations) are mutually exclusive
};

struct DataFlags {
	u32 flags;

	DataFlags(u32 flags);
	bool isAddress();
	DataFlags getLocation();

	operator u32&() { return this->flags; }
	u32* operator&() { return &this->flags; }
};

// Data is a struct which holds *in-flight* information which flows from
// one part of the machine to another. It represents a transient value,
// and therefore, storing data is unique to a software simulator, but is
// likely unnecessary with a hardware implementation.
struct Data {
	size_t val;
	u32 len; // Encodes the length of the value to be used in the instruction
	         // This is either the length of the literal or the length of the
	         // value being pointed to. The length of the address is not
	         // required to be stored. Addresses are always of a fixed length
	         // on a particular system.

	Data();
	Data(const void* insnbits, u32 dataOffset, u32 dataLen);

	u64 get(u8 ofSize);
	u64 getVal(Stack& spm, Regfile& reg, u8 memory[]);
	void resize(u32 newSize, bool isSigned = false);
};

void signExtend(u8* num, const u32 oldBitLength, const u32 newBitLength);

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

	Stack* spm;
	Regfile& regs;
	u8 wordSize; // in bits

public:
	Stack(
		u32 bufferSize, u8* memory, Stack* spmStack, Regfile& regfile, u16 wordSize
	);
	~Stack();

	bool push(Data data, DataFlags flags);
	bool push(const void* data, u32 length, DataFlags flags);
	void clear();
	Data get(u32 index, u8 size = 0, bool isSigned = false);
	void* getData(u32 index);
	bool write(u32 index, void* val, u32 size);
	void replace(u32 index, void* val, u32 size);
	DataFlags getFlags(u32 index);
	u32 getLen(u32 index);

	void printStatus();
};
#endif // DATA_H
