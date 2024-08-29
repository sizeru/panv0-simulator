/*
 * This file describes the special purpose registers of PAN. Special
 * purpose registers may be of a fixed size. This size is known to the
 * architecture, but never explicitly encoded in the ISA.
 */
#include "regfile.h"

Regfile::Regfile(addr entryPoint, addr topOfStack) {
	for (int i = 0; i < REG__MAX; i++) {
		this->regs[i].size = 8;
		this->regs[i].val = 0;
	}
	this->regs[REG_SP].val = topOfStack;
	this->regs[REG_FP].val = topOfStack;
	this->regs[REG_LR].val = entryPoint;
	this->regs[REG_PC].val = entryPoint;
	this->regs[REG_ANC].val = entryPoint;
	return;
}
