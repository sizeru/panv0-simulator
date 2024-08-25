#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include "shorttypes.h"

enum DataFlag {
	// The following flags are mutually exclusive
	DATA_IS_LITERAL = 0x0,
	DATA_IS_ADDRESS = 0x1,
	// The previous flags are mutually exclusive
	// The following flags are mutually exclusive
	DATA_IS_INTEGER = 0x2,
	DATA_IS_SPM = 0x4,
	DATA_IS_REG = 0x6,
	// The previous flags are mutually exclusive
};

typedef u32 DataFlags;

struct Data {
	u32 length;
	union {
		size_t val;
		u8* ptr;
	} contents;
	DataFlags flags;

	Data();
	Data(void* instr, u32 suffixLen, u32 opLen, u32 instrLen);
	~Data();
};

#endif // DATA_H
