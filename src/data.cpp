#include "data.h"
#include <cstring>
#include <stdio.h>

Data::Data() : length(0), flags(0) {}

// Extracts the data (excluding flags) from an instruction
Data::Data(void *instr, u32 suffixLen, u32 opLen, u32 instrLen) {
	u32 dataLen = (instrLen * 8) - opLen - suffixLen; // length in bits
    printf("instrLen: %d, opLen: %d, suffixLen: %d\n", instrLen * 8, opLen, suffixLen);
	// Ensure dataLen is always a multiple of 2 and is stored in bytes
	u32 rounded = dataLen;
	rounded |= rounded >> 1;
	rounded |= rounded >> 2;
	rounded |= rounded >> 4;
	rounded |= rounded >> 8;
	rounded |= rounded >> 16;
	rounded += 1;
	rounded >>= 3;
	this->length = rounded;
	u8* data;
	if (rounded > 8) {
		this->contents.ptr = new u8[(8*rounded + 7) / 8]();
		data = (u8*)this->contents.ptr;
	} else {
		this->contents.val = 0;
		data = (u8*)&this->contents.val;
	}

	// Copy data
	dataLen = (dataLen+7) / 8;
	u8* dataPos = (u8*)instr + (suffixLen+opLen)/8;
	u8 dataOffset = (suffixLen + opLen) % 8;

	// TODO: Segfault here
	data[0] = dataPos[0] >> dataOffset;

	for (int i = 1; i < (dataLen+7)/8; i++) {
		data[i-1] |= dataPos[i] >> (8-dataOffset);
		data[i] |= dataPos[i] >> dataOffset;
	}
	return;
}

Data::~Data() {
	if (this->length > sizeof(size_t)) {
		delete[] this->contents.ptr;
	}
}
