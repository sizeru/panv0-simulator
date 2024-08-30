#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include "shorttypes.h"
#include "regfile.h"

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

	operator u32 &() { return this->flags; }
  u32* operator &() { return &this->flags; }
};


// Data is a struct which holds *in-flight* information which flows from
// one part of the machine to another. It represents a transient value,
// and therefore, storing data is unique to a software simulator, but is
// likely unnecessary with a hardware implementation. 
struct Data {
	union {
		size_t val;
		u8* ptr;
	} contents;
	DataFlags flags;
	u32 length; // Encodes the length of the value to be used in the instruction
	            // This is either the length of the literal or the length of the
							// value being pointed to. The length of the address is not
							// required to be stored. Addresses are always of a fixed length
							// on a particular system.

	Data();
	Data(void* instr, u32 dataOffset, u32 dataLen, bool signExtend);

	u64 getVal(Stack& spm, Regfile& reg, u8 memory[]);
	~Data();
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

public:
	Stack(u32 bufferSize, u8* memory, Stack* spmStack, Regfile& regfile);
	~Stack();

	bool push(Data data);
	bool push(void* data, u32 length, DataFlags flags);
	void clear();
	Data get(u32 index);
	void* getData(u32 index);
};
#endif // DATA_H
