/*
 * NOTE: This currently only supports instructions up to 8 bytes
 */
#include <cstddef>
#include <cstring>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "data.h"
#include "instruction.h"
#include "panresult.h"
#include "shorttypes.h"

struct ExecStackLine {
	Data data;
	bool isAddress;
};

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		puts("./panv0 [-f <inputfile>] "
		     "[-e <execution-stack-buffer-size>] [-s <spm-reg-buffer-size>] "
		     "[-m <memory-size>] [-w <word-size>]");
		return 1;
	}

	char* filename = NULL;
	size_t execStackBufferSize = 4096;
	size_t spmRegBufferSize = 4096;
	size_t memSize = 4096;
	u16 wordSize = 16;
	// Update defaults with user passed args
	int flag;
	while ((flag = getopt(argc, argv, "f:e:s:m:w:")) != -1) {
		switch (flag) {
		case 'f': filename = optarg; break;
		case 'e': execStackBufferSize = atol(optarg); break;
		case 's': spmRegBufferSize = atol(optarg); break;
		case 'm': memSize = atol(optarg); break;
		case 'w': wordSize = atol(optarg); break;
		}
	}

	panAddress entry = KiB(1);
	panAddress returnPoint = 128;

	// Build computer out of components (data structures)
	u8* memory = new u8[memSize];
	Regfile regs(entry, returnPoint, memSize - 8);
	Stack spmStack(spmRegBufferSize, memory, nullptr, regs, wordSize);
	Stack prepStack(spmRegBufferSize, memory, &spmStack, regs, wordSize);
	Stack execStack(execStackBufferSize, memory, &spmStack, regs, wordSize);

	// The starting point and size of the stack
	// size_t stackBottom = KiB(48);
	// size_t stackTop = KiB(16);
	size_t haltAddress = regs[REG_LR].val;
	u8* imem = memory + entry;
	size_t imemSize = KiB(7);

	// Load file into buffer
	FILE* file = fopen(filename, "r");
	if (file == nullptr) {
		printf("File: '%s' does not exist\n", filename);
		return 2;
	}

	// Allocate buffer for instructions
	size_t bytes_read = fread(imem, 1, imemSize, file);
	printf(
		"Read %lu bytes from '%s' into instruction memory\n", bytes_read, filename
	);
	if (bytes_read == 0) {
		panExit(PAN_ERR_INSTRUCTION_READ);
	}

	// Stream the file into the buffer
	printf("Running program beginning from entry point: %lx\n\n", entry);
	while (regs[REG_PC].val != haltAddress) {
		printf("PC: 0x%lx\n", regs[REG_PC].val);
		// Get different instruction fields
		Instruction instr(memory, regs[REG_PC]);
		printf(
			"\tRead %u B instruction: %s %lu [",
			instr.len,
			opNames[instr.op],
			instr.data.val
		);
		printbits(memory + regs[REG_PC].val, instr.len);
		printf("]\n");
		instr.predictPC(regs[REG_PC]);
		bool ok = instr.run(execStack, prepStack, spmStack, regs, memory, wordSize);
		if (!ok) {
			printf("Run terminated prematurely\n");
			return -1;
		}
	}
	printf("Program terminated succesfully. (reached expected return point)\n");
}
