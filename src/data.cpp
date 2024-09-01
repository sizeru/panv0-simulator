#include "data.h"
#include "instruction.h"
#include <cstring>
#include <stdio.h>

Data::Data() : len(0), val(0) {}

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
Data::Data(const void *insnbits, u32 dataOffset, u32 dataLen)
	: val(bfe(*reinterpret_cast<const u64 *>(insnbits), dataOffset, dataLen)),
	  len(dataLen) {}

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

void Data::resize(u32 newSize, bool isSigned) {
	if (newSize < sizeof(void*)*8) {
		this->val &= (1UL << newSize) - 1;
	}
	if (isSigned) {
		signExtend(reinterpret_cast<u8*>(&this->val), this->len, newSize);
	}
}