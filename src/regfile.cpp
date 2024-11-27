/*
 * This file describes the special purpose registers of PAN. Special
 * purpose registers may be of a fixed size. This size is known to the
 * architecture, but never explicitly encoded in the ISA.
 */
#include "regfile.h"
#include "shorttypes.h"

Regfile::Regfile(
	panAddress entryPoint, panAddress returnPoint, panAddress topOfStack
) {
	for (int i = 0; i < REG__MAX; i++) {
		this->regs[i].size = 12; // Size typically equal to size of memory
		this->regs[i].val = 0;
	}
	this->regs[REG_SP].val = topOfStack;
	this->regs[REG_FP].val = topOfStack;
	this->regs[REG_LR].val = returnPoint;
	this->regs[REG_PC].val = entryPoint;
	this->regs[REG_ANC].val = 0;
	return;
}
