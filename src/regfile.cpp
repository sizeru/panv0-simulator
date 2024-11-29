/*
 * This file describes the special purpose registers of PAN. Special
 * purpose registers may be of a fixed size. This size is known to the
 * architecture, but never explicitly encoded in the ISA.
 */
#include "regfile.h"
#include "shorttypes.h"

Register* Regfile::operator&() { return this->regs; }


Register& Regfile::operator[](u64 index) {
  // Simulate register size by always truncating contents
  // this->regs[index].val &= (1UL << this->regs[index].size) - 1;
  return this->regs[index];
}

Regfile::Regfile(u64 wordSize) {
	for (int i = 0; i < REG__MAX; i++) {
		this->regs[i].size = wordSize; // Size typically equal to size of memory
		this->regs[i].val = 0xA;
	}
}

Regfile::Regfile(
	u64 wordSize, panAddress entryPoint, panAddress returnPoint,
	panAddress topOfStack
) : Regfile(wordSize) {
	this->regs[REG_SP].val = topOfStack;
	this->regs[REG_FP].val = topOfStack;
	this->regs[REG_LR].val = returnPoint;
	this->regs[REG_PC].val = entryPoint;
	this->regs[REG_ANC].val = 0;
	return;
}
