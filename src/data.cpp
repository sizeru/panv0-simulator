#include "data.h"
#include "instruction.h"
#include <cstring>
#include <stdio.h>

Data::Data() : length(0), flags(0) {
	this->contents.val = 0;
}

// Round up to the nearest power of two. Most C primitives are powers of two.
u32 roundUpPow2(u32 val) {
	u32 r = val - 1; // byte length data r up to nearest power of 2
	r |= r >> 1;
	r |= r >> 2;
	r |= r >> 4;
	r |= r >> 8;
	r |= r >> 16;
	return r += 1;

}

/**
 * @brief Extracts the data (excluding flags) from an instruction
 *
 * @param instr A pointer to the beginning of the instruction
 * @param dataOffset The offset of the data section from the start of
 * the instruction in bits
 * @param dataLen The length of the instruction in bits
 * @param signExtend Should the data be sign extended?
 */
Data::Data(void *instr, u32 dataOffset, u32 dataLen, bool shouldSignExtend) : flags(0) {

	// Calculate the size of the data we will use by rounding up to
	// nearest power of 2. This will likely result in data expansion,
	// which is convenient for use as operands later.
	u32 dataByteLength = (dataLen + 7) / 8;
	u32 roundedDataByteLength = roundUpPow2(dataByteLength);
	this->length = roundedDataByteLength;

	// Decide where the information from this instruction should be
	// stored.
	u8* data;
	if (this->length > 8) {
		// Data is too long to fit in this 
		this->contents.ptr = new u8[this->length]();
		data = (u8*)this->contents.ptr;
	} else {
		this->contents.val = 0;
		data = (u8*)&this->contents.val;
	}

	// Copy the data from this instruction
	u8* dataPos = (u8*)instr + dataOffset/8;
	u8 dataByteOffset = dataOffset % 8;
	// Copy first bit
	data[0] = dataPos[0] >> dataByteOffset;
	// Other bits
	for (int i = 1; i < (dataLen+7)/8; i++) {
		data[i-1] |= dataPos[i] >> (8-dataByteOffset);
		data[i] |= dataPos[i] >> dataByteOffset;
	}

	// Sign extend if necessary
	if (shouldSignExtend) {
		signExtend(data, dataLen, this->length * 8);
	}
	return;
}

Data::~Data() {
	if (this->length > sizeof(size_t)) {
		delete[] this->contents.ptr;
	}
}

DataFlags::DataFlags(u32 flags) : flags(flags) {}

bool DataFlags::isAddress() {
	return this->flags & 1;
}

DataFlags DataFlags::getLocation() {
	return DataFlags(this->flags & 6);
}

void signExtend(u8* num, const u32 oldBitLength, const u32 newBitLength) {
	u8 byteIndex = (oldBitLength - 1) / 8;
	u8 byteOffset = (oldBitLength - 1) % 8;

	u8 topBit = num[byteIndex] & (1 << byteOffset) ? 1 : 0;

	for (unsigned i = oldBitLength; i < newBitLength; i++) {
		byteIndex = i / 8;
		byteOffset = i % 8;
		if (topBit) {
			num[byteIndex] |= topBit << byteOffset;
		} else {
			num[byteIndex] &= ~(topBit << byteOffset);
		}
	}
	return;
}

u64 Data::getVal(Stack& spm, Regfile& regs, u8 memory[]) {
	u64 val = 0;
	u32 sizeOfVal = 0;
	switch (this->flags.getLocation()) {
		case DATA_IS_INTEGER: val = this->contents.val; break;
		case DATA_IS_REG: val = regs[this->contents.val].val; break;
		case DATA_IS_SPM: val = spm.get(this->contents.val).contents.val; break;
		default: break;
	}
	if (this->flags.isAddress()) {
		val = *(u64*)(memory + val) & ((1UL << this->length) - 1);
	}
	return val;
}