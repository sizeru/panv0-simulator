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

## FAQ

- What does it mean to do a 4B add and store the result in an 8B memory location?
   * It means the top 4 bytes are zeroed.
- What are SPM registers
   * SPM registers are used to store transient values. SPM is short for
     scratch-pad memory.
   * A SPM register can store either a literal or a pointer. Should it
     store a literal, is also stores the length of the literal. Should
     it store a pointer, the length of a pointer is already known to the
     machine, so it instead stores the size of the data it is pointing
     to.
   * Regardless of the kind of data stored by the spm register, the spm
     register can be treated as either a literal or a reference like any
     other.
   * The first time a SPM reg is a destination requires slightly
     different syntax. This is because we must indicate what kind of
     data the spm register holds, and its size. This is an
     initialization step.
   * Debating making this into a separate instruction. But it does not
     seem difficult for the hardware. So perhaps a psuedo instruction.
     `INIT(A) SPM0 4` perhaps?

## Reasons a run may not complete

- Stack grows beyond bounds

## TODO
- Need to make SPM register indices relative.
- The return value is SPM0
- The args are < SPM0
- Reading from and writing to the offset of an address is useful to do in
  a single instruction. Make the ISA support this.
- Add INIT psuedo-op: INIT SPM4 = PUSHA SPM4
