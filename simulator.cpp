/*
 * NOTE: This currently only supports instructions up to 8 bytes
 */
#include <cstdint>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <vector>
#include <unistd.h>

enum instruction {
	PUSHIL,
	PUSHIA,
	PUSHSL,
	PUSHSA,
	PUSHRL,
	PUSHRA,
	ADD,
	JUMPABS,
	JUMP
};


struct ExecStackLine {
	enum instruction instr;
	void* data;
	void* dataLen; // Length of data in bits
};
/*
Steps to simulate PANv0
1. Read in an array of bytes from a file
*/
// Read in an array of bytes from a file

void printUsage() {
	// TODO: Write me
	return;
};

bool validArgs(int argc, char* argv[]) {
	// TODO: Write me
	return true;
}

// Count trailing ones of an arbitrary data type
uint16_t cto(void* insn) {
	uint16_t ones_count = 0;
	while (1) {
		uint8_t byte = *(uint8_t*)insn;
		for (int i = 0; i < 8; i++) {
			if (!(byte & (1 << i))) {
				return ones_count;
			}
		}
		insn = (char*)insn + 1;
	}
}

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		printUsage();
		return 1;
	}

	char* filename = NULL;
	size_t bufferSize = 4096;
	size_t execStackBufferSize = 8;
	size_t spmRegBufferSize = 8;
	size_t cacheBufferSize = 8;
	// Update defaults with user passed args
	int flag;
	while ((flag = getopt(argc, argv, "f:b:e:s:c:"))) {
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

	// Allocate buffer
	unsigned char buffer[bufferSize];
	size_t cursor = -1; // position in buffer
	size_t bytes_read = 0;

	// Load file into buffer
	FILE* file = fopen(filename, "r");

	// Read the file into the buffer continuously
	std::vector<ExecStackLine> execStack;
	while ((bytes_read != fread(buffer, bufferSize, 1, file))) {
		while (cursor < bytes_read) {
			size_t instr = *(size_t*)(buffer + cursor);
			

		}
		uint64_t instr = buffer[cursor];
	}
}
