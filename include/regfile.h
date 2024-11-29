#ifndef REGFILE_H
#define REGFILE_H
/*
 * This file describes the special purpose registers of PAN. Special
 * purpose registers may be of a fixed size. This size is known to the
 * architecture, but never explicitly encoded in the ISA.
 */
#include "shorttypes.h"

enum RegName {
	REG_SP,  // Stack Pointer
	REG_FP,  // Frame Pointer
	REG_LR,  // Link Regitser
	REG_PC,  // Program Counter
	REG_ANC, // Anchor Register
	REG__MAX
};

static const char* regNames[REG__MAX] = {"SP", "FP", "LR", "PC", "ANC"};

struct Register {
	u8 size;
	u64 val;
};

// A regfile in PANv0 contains only the special purpose registers. Special
// purpose registers are of a fixed size, based on the word size. Word size is
// not guaranteed to be a power of 2. A regfile acts fundamentally identically
// to an array of registers.
struct Regfile {
	Register regs[REG__MAX];

	// Give a pointer to the registers
	Register* operator&();
	// Convenient way of accessing a register
	Register& operator[](u64 index);
	Regfile(u64 wordSize);
	Regfile(
		u64 wordSize, panAddress entryPoint, panAddress returnPoint,
		panAddress topOfStack
	);
};

#endif /* REGFILE_H */
