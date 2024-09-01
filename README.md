## PANv0 Simulator
A simulator for the PANv0 computer architecture.

## Description
PAN is an stack-based architecture which seeks to create highly portable
binaries. This is a reference implementation of the PANv0 ISA
architecture in software.

## Installation
```
make
```

## Usage
```
./panv0 [-f <inputfile>] [-b <bufferSize>] [-e <execution-stack-buffer-size>] [-s <spm-reg-buffer-size>] [-c <cache-buffer-size>]
```

## Implementation Details
In this implementation, by default:
- Memory is 64kB
- The stack starts at 48kB
- The stack ends at 16kB
- Instructions take up space from 1kB to 16kB.
- Space above the stack is used as overflow space for on chip stacks.
- 4kB spm stack
- 1kB exec stack
- 256B distinct prep stack used for prep instructions

The simple data flow of an instruction (in this implementation) is as
follows:
1. A buffer of instruction bits is read in from memory.
2. The instruction's op and data is extracted (data is rounded up to
   nearest power of 2).
3. (Optional) The instruction's details are stored to the appropriate
   stack
4. The data from the stack along with the instruction's details are used
   to run the instruction.

## Questions

- What does it mean to do a 4B add and store the result in an 8B memory location?
- It means the top 4 bytes are zeroed.