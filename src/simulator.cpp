/*
 * NOTE: This currently only supports instructions up to 8 bytes
 */
#include <cstdint>
#include <cstring>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#include "stack.h"
#include "data.h"
#include "instruction.h"

struct ExecStackLine {
	Data data;
	bool isAddress;
};

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		puts("./panv0 [-f <inputfile>] [-b <bufferSize>] "
				 "[-e <execution-stack-buffer-size>] [-s <spm-reg-buffer-size>] "
				 "[-c <cache-buffer-size>]");
		return 1;
	}

	char *filename = NULL;
	size_t bufferSize = 4096;
	size_t execStackBufferSize = 4096;
	size_t spmRegBufferSize = 4096;
	size_t cacheBufferSize = 4096;
	// Update defaults with user passed args
	int flag;
	while ((flag = getopt(argc, argv, "f:b:e:s:c:")) != -1) {
		switch (flag) {
		case 'f':
			filename = optarg;
			break;
		case 'b':
			bufferSize = atol(optarg);
			break;
		case 'e':
			execStackBufferSize = atol(optarg);
			break;
		case 's':
			spmRegBufferSize = atol(optarg);
			break;
		case 'c':
			cacheBufferSize = atol(optarg);
			break;
		}
	}

	// Create stacks
	u8 memory[1024*8];
	Stack spmStack(spmRegBufferSize, memory);
	Stack execStack(execStackBufferSize, memory);

	// Allocate buffer for instructions
	unsigned char buffer[bufferSize];
	size_t cursor = 0; // position in buffer
	size_t bytes_read = 0;

	// Load file into buffer
	FILE *file = fopen(filename, "r");
	if (file == nullptr) {
		printf("File: '%s' does not exist\n", filename);
		return 2; 
	}

	// Stream the file into the buffer
	while ((bytes_read = fread(buffer, 1, bufferSize, file)) != 0) {
		printf("Read %lu bytes into buffer\n", bytes_read);
		while (cursor < bytes_read) {
			// Get different instruction fields
			u8* instrPos = buffer + cursor;
			// printf("Read Instr with starting byte: 0x%02x\n", *instrPos);
            printf("HERE1\n");
			Instruction instr = getInstr(instrPos);
            printf("HERE2\n");
			runInstr(instr, execStack, spmStack, memory);
            printf("HERE3\n");
			cursor += instr.len;
		}
	}
}
