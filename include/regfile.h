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
	REG_PC,  // Program Counter
	REG_LR,  // Link Regitser
	REG_ANC, // Anchor Register
	REG__MAX
};

struct Register {
	u8 size;
	u64 val;
};

struct Regfile {
	Register regs[REG__MAX];

  Register* operator &() { return this->regs; }
	Register& operator[](std::size_t index) { return this->regs[index]; }
	Regfile(addr entryPoint, addr topOfStack);
};

#endif /* REGFILE_H */