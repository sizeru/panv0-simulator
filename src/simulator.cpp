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

void printUsage(const char*);
u8* flashMemory(u64 memSize, u64 entry, u64 imemSize, const char* filename);
bool run(
	const Instruction& instr, Regfile& regs, u8* mem, Stack& spm, Stack& eval
);
void predictPC(const Instruction& instr, Register& pc);

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		puts("ERROR: No program arguments found. Use the -h option for help");
		return 1;
	}

	// Deciding on arguments. Create defaults for what this machine can do.
	char* filename = NULL;
	size_t evalStackBufferSize = 512;
	size_t spmRegBufferSize = 512;
	size_t memSize = 4096;
	size_t imemSize = 1024;
	// size_t rl0Size = 128;
	u16 wordSize = 12; // no MMU, so make word size same as vmem size

	int flag;
	while ((flag = getopt(argc, argv, "hf:e:s:m:w:")) != -1) {
		switch (flag) {
		case 'h': printUsage(argv[0]); return 0;
		case 'i': imemSize = atoi(optarg); break;
		case 'f': filename = optarg; break;
		case 'e': evalStackBufferSize = atol(optarg); break;
		case 's': spmRegBufferSize = atol(optarg); break;
		case 'm': memSize = atol(optarg); break;
		// case 'r': rl0Size = atol(optarg); break;
		case 'w': wordSize = atol(optarg); break;
		}
	}

	if (filename == NULL) {
		puts("ERROR: No file provided. Use -h for help");
		return 1;
	}

	// Creating the initial state of the machine. This is used to determine when
	// execution has terminated, the bounds of the stack, etc.
	// TODO: Always useful to have more info in this initial state, such as:
	// - lower bound of the stack
	// - size of imem vs dmem
	// Although, this info is sometimes set by a startup program...
	panAddress entry = 32;
	u8* memory = flashMemory(memSize, entry, imemSize, filename);
	panAddress returnPoint = 16;
	panAddress topOfStack = memSize - 8;

	Regfile regs(wordSize);
	regs[REG_SP].val = topOfStack;
	regs[REG_LR].val = returnPoint;
	regs[REG_PC].val = entry;
	regs[REG_ANC].val = 0;

	// Cache rl0Cache(rl0Size, 1); // RL0 Cache - Used to dynamically allocate
	// registers. Not necessary for correctness. Will implement later.
	Stack spmStack(spmRegBufferSize, memory, nullptr, regs, wordSize);
	Stack evalStack(evalStackBufferSize, memory, &spmStack, regs, wordSize);

	// The starting point and size of the stack
	// size_t stackBottom = KiB(48);
	// size_t stackTop = KiB(16);
	size_t haltAddress = regs[REG_LR].val;

	u64 cycle = 1;
	u64 maxCycle = 1000;

	printf("Running program beginning from entry point: %lx\n\n", entry);
	while (regs[REG_PC].val != haltAddress && cycle < maxCycle) {
		printf("PC: 0x%lx (cycle: %lu)\n", regs[REG_PC].val, cycle);
		// Get different instruction fields
		Instruction instr = Instruction::fetch(memory, regs[REG_PC]);
		printf(
			"\tRead %u B instruction: %s %lu [",
			instr.len,
			opNames[instr.op],
			instr.data.val
		);
		printbits(memory + regs[REG_PC].val, instr.len);
		printf("]\n");
		predictPC(instr, regs[REG_PC]);

		bool ok = run(instr, regs, memory, spmStack, evalStack);
		if (!ok) {
			printf("Run terminated prematurely\n");
			return -1;
		}
		cycle++;
	}

	if (cycle == maxCycle) {
		puts("Max cycle reached");
		exit(1);
	}

	puts("Program terminated succesfully. (reached expected return point)\n\n");
	printf("Final state of SPM Space\n");
	spmStack.printStatus();
	printf("Final state of regs\n");
	for (int i = 0; i < REG__MAX; i++) {
		printf("\t%s: %lx\n", regNames[i], regs[i].val);
	}
	printf("Final state of used Memory Stack\n");
	for (int addr = (regs[REG_SP].val / 8) * 8; addr < topOfStack; addr++) {
		if (addr % 8 == 0) {
			printf("\t%x: ", addr);
		}

		printf("%02x ", *(memory + addr));

		if ((addr + 1) % 8 == 0) {
			printf("\n");
		}
	}
}

void printUsage(const char* programName) {
	printf(
		"Usage: %s [OPTION]... -f <INPUT-BINARY>\n"
		"An interpretor for the PANv0 ISA. This used to evaluate the\n"
		"functionality and performance of the ISA. It is expected that\n"
		"<INPUT-BINARY> is a file which contains compiled PANv0 code.\n"
		"\n"
		"  -w <WORD-SIZE>         Specify the word size of this simulator. This\n"
		"                         is used by the microarchitecture to determine\n"
		"                         the size of a pointer and of virtual memory\n"
		"  -r <RL0-CACHE-SIZE>    The size in Bytes of the lowest level cache.\n"
		"                         in the system. PANv0 is a memory-to-memory ISA,\n"
		"                         but data in this cache is expected to have\n"
		"                         similar latencies as traditional registers\n"
		"  -s <SPM-SIZE>          How many Bytes to dedicate to SPM (Scratch-Pad\n"
		"                         Memory) on this simulated processor. SPM space\n"
		"                         is necessary to hold intermediate calculations\n"
		"                         which do not have a space in memory\n"
		"  -e <EVAL-STACK-SIZE>   The size of the evaluation stack in Bytes.\n"
		"                         The eval stack is a space used to compose a\n"
		"                         mathematical statement (ie. declaring the\n"
		"                         operands and operator of an expression).\n"
		"                         Overflowing the eval stack will throw an\n"
		"                         exception which is handled by the OS. It may\n"
		"                         choose to stream in the operands, or not.\n"
		"  -m <MEMORY-SIZE>       The size of memory in Bytes. Memory may be used\n"
		"                         as a fallback when the processor runs out of\n"
		"                         microarchitectural space.\n"
		"  -i <IMEM-SIZE>         The about of space in BYTES to reserve for\n"
		"                         instruction memory. The rest of the space in\n"
		"                         memory will be reserved for data (in this\n"
		"                         implementation, the only data in memory is the\n"
		"                         stack.\n",
		programName
	);
}

// Place a program into the correct location of memory
u8* flashMemory(u64 memSize, u64 entry, u64 imemSize, const char* filename) {
	printf(
		"Flashing file '%s' into memory.\n"
		"- %luB of memory.\n"
		"- entry point is at %lu\n"
		"- %luB reserved for imem)\n",
		filename,
		memSize,
		entry,
		imemSize
	);

	u8* memory = new u8[memSize];

	FILE* file = fopen(filename, "r");
	if (file == nullptr) {
		panExit(PAN_ERR_INVALID_FILE);
	}

	// Allocate buffer for instructions
	u8* imem = memory + entry;
	size_t bytes_read = fread(imem, 1, imemSize, file);
	printf(
		"Read %lu bytes from '%s' into instruction memory\n", bytes_read, filename
	);
	if (bytes_read == 0 || bytes_read == imemSize) {
		panExit(PAN_ERR_INSTRUCTION_READ);
	}

	return memory;
}

bool run(
	const Instruction& instr, Regfile& regs, u8* mem, Stack& spm, Stack& eval
) {

	static u64 sizeOfTarget; // Will we PUSH a literal (0) or an address (!0)
	// static u64 maxSizeOfTarget = regs[0].size; // Impossible to dereference
	// outside of memory bounds

	switch (instr.op) {
	case OP_DSIZE:
		if (sizeOfTarget != 0) {
			printf("Error: Size of target is not 0! It's possible that two DSIZE\n"
			       "instructions have followed each other\n");
			return false;
		}
		sizeOfTarget = instr.data.val * 8;
		return true;
	case OP_PUSHI:
	case OP_PUSHS:
	case OP_PUSHR: {
		DataFlags flags = sizeOfTarget == 0 ? DATA_IS_LITERAL : DATA_IS_ADDRESS;
		flags |= static_cast<DataFlag>(instr.op << 1); // Stupid bit twiddle hack.
		                                               // Not required.
		u32 dataLen;
		if (flags.isAddress()) {
			dataLen = sizeOfTarget;
		} else {
			dataLen = instr.data.len;
		}
		sizeOfTarget = 0;

		printf(
			"\tStack::push() -> Pushing %u bit %s value of 0x%lx to stack\n",
			dataLen,
			flags.isAddress() ? "address" : "literal",
			instr.data.val
		);
		bool ok = eval.push(&instr.data.val, dataLen, flags);
		if (!ok) {
			puts("\tFailed to push value to eval stack");
		}
		return ok;
	}
	// ADD <size> [dst] [op1] [op2] [shift]
	case OP_ADD: {
		u64 resLen = instr.data.val == 0 ? eval.getLen(0) : instr.data.val * 8;
		Data a = eval.get(1, resLen, true);
		Data b = eval.get(2, resLen, true);
		Data shift = eval.get(3, resLen);

		u64 result = a.val + (b.val << shift.val);
		if (resLen < 64 && resLen != 0) { // TODO: Is this necessary?
			result &= (1UL << resLen) - 1;
		}
		printf(
			"\t(%lub ADD) %lx = %lx + (%lx << %lx)\n",
			resLen,
			result,
			a.val,
			b.val,
			shift.val
		);
		bool ok = eval.write(0, &result, resLen);
		eval.clear();
		return ok;
	}
	case OP_JUMPABS: {
		u64 wordSize = regs[REG_SP].size;
		Data condCode = instr.data;
		Data target = eval.get(0, wordSize); // where to jump to
		Data op1 = eval.get(1);
		Data op2 = eval.get(2);
		// TODO: Add support for other cond codes
		if (op1.val == op2.val) {
			regs[REG_PC].val = target.val;
		} else {
			regs[REG_PC].val += instr.len;
		}
		printf(
			"\t(JUMP.ABS): cond: %lx target: %lx, op1: %li, op2: %lu\n",
			condCode.val,
			target.val,
			op1.val,
			op2.val
		);
		eval.clear();
		return true;
	}
	case OP_JUMP: {
		u64 wordSize = regs[REG_SP].size;
		Data offset = instr.data;
		offset.resize(wordSize, true);
		Data cond = eval.get(0);
		Data op1 = eval.get(1);
		Data op2 = eval.get(2);
		if (op1.val == op2.val) {
			regs[REG_PC].val =
				(regs[REG_PC].val + offset.val) & ((1UL << wordSize) - 1);
		} else {
			regs[REG_PC].val += instr.len;
		}
		offset.resize(64, true);
		printf(
			"\tjumpOffset: %li, cond: %lx, op1: %li, op2: %lu\n",
			offset.val,
			cond.val,
			op1.val,
			op2.val
		);
		eval.clear();
		return true;
	}
	default: // Unknown instruction
		puts("\tTrying to run unknown instruction");
		return false;
		break;
	}
	return true;
}

void predictPC(const Instruction& instr, Register& pc) {
	switch (instr.op) {
	// JUMP instructions will figure out their PC during execution
	case OP_JUMP: // pc.val += instr.data.val; break;
	case OP_JUMPABS:
		// A more complex deref needs to happen here
		// pc.val += instr.len;
		break;
	default: pc.val += instr.len; break;
	}
	return;
}
