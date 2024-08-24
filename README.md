## PANv0 Simulator
A simulator for the PANv0 computer architecture.

## Description
PAN is an stack-based architecture which seeks to create highly portable binaries. 

## Installation
```
make
```

## Usage
```
./panv0 [-f <inputfile>] [-b <bufferSize>] [-e <execution-stack-buffer-size>] [-s <spm-reg-buffer-size>] [-c <cache-buffer-size>]
```

## Structure of Stack

```
^  
| Length (4B - 4B align)
|
| Contents (8B Align)
|
| Length (4B - 4B align)
0
```