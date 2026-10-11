# 16-Bit LC-3 Virtual Machine & Assembler in C

A complete, from-scratch project implementing the **LC-3 (Little Computer 3)** architecture in pure C. This repository contains both an execution engine (**Virtual Machine**) and a bytecode compiler (**Two-Pass Assembler**), providing a full vertical computing stack: from writing human-readable assembly language, to compiling it into binary object files, to executing it on a software-emulated 16-bit processor.

---

## Table of Contents
- [Overview](#overview)
- [Architecture Specification](#architecture-specification)
  - [Memory Layout](#memory-layout)
  - [Registers](#registers)
  - [Condition Codes](#condition-codes)
  - [Endianness](#endianness)
- [System Components](#system-components)
  - [1. The Virtual Machine (`vm`)](#1-the-virtual-machine-vm)
  - [2. The Two-Pass Assembler (`assembler`)](#2-the-two-pass-assembler-assembler)
- [Instruction Set Reference](#instruction-set-reference)
- [Building the Project](#building-the-project)
- [Usage & Workflow](#usage--workflow)
  - [1. Write Assembly Code](#1-write-assembly-code)
  - [2. Assemble to Machine Code](#2-assemble-to-machine-code)
  - [3. Run the Virtual Machine](#3-run-the-virtual-machine)
- [References & Acknowledgments](#references--acknowledgments)
- [License](#license)

---

## Overview

Modern software development often hides the underlying interactions between compilers, operating systems, and hardware. This project explores that abstraction boundary by building two interconnected components:

1. **The Assembler (`assembler.c`):** A two-pass translator that parses LC-3 assembly text, tracks memory addresses, resolves symbolic forward/backward labels, validates ranges, and emits standard Big-Endian `.obj` binary files.
2. **The Virtual Machine (`vm.c`):** A cycle-accurate emulator modeling the Von Neumann architecture. It loads `.obj` files into word-addressable memory, runs a continuous Fetch-Decode-Execute cycle, manages register states, handles Memory-Mapped I/O, and provides low-level terminal integration.

---

## Architecture Specification

### Memory Layout
LC-3 memory is **word-addressable** ($2^{16} = 65,536$ addressable locations), where each address stores a 16-bit word:

| Address Range | Allocation | Description |
| :--- | :--- | :--- |
| `0x0000 – 0x00FF` | **Trap Vector Table (TVT)** | Pointers to operating system service routines |
| `0x0100 – 0x01FF` | **Interrupt Vector Table (IVT)**| Pointers to hardware interrupt service routines |
| `0x0200 – 0x2FFF` | **Operating System Space** | Reserved for kernel code, handlers, and tables |
| `0x3000 – 0xFDFF` | **User Program Space** | General user programs, code, and static memory |
| `0xFE00 – 0xFFFF` | **Device Registers (MMIO)** | Memory-Mapped I/O (`KBSR`, `KBDR`, `DSR`, `DDR`) |

### Registers
The processor maintains 10 internal 16-bit registers:
- **General-Purpose Registers (`R0`–`R7`):** 8 registers used for arithmetic, logic, and data storage.
- **Program Counter (`PC`):** Holds the address of the next instruction to fetch.
- **Processor Status Register (`PSR` / `COND`):** Tracks processor privilege level and the condition codes of the most recent calculation.

### Condition Codes
Every instruction that modifies a destination register updates the condition flags based on a two's complement interpretation of the resulting value:
- **`FL_POS` ($P = 001_2$):** Value is strictly positive ($> 0$).
- **`FL_ZRO` ($Z = 010_2$):** Value is zero ($= 0$).
- **`FL_NEG` ($N = 100_2$):** Value is strictly negative ($< 0$, bit 15 is 1).

### Endianness
LC-3 binary `.obj` files on disk are structured in **Big-Endian** (Most Significant Byte first). Because most host processors (x86-64, modern ARM) are Little-Endian, the assembler swaps 16-bit integers to Big-Endian when writing to disk, and the VM converts them back to host endianness upon reading.

---

## System Components

### 1. The Virtual Machine (`vm`)
The virtual machine executes compiled machine code through the following sub-systems:

- **Fetch-Decode-Execute Cycle:**
  - *Fetch:* Fetches the 16-bit instruction at `memory[PC++]`.
  - *Decode:* Extracts the 4-bit opcode (`ins >> 12`).
  - *Execute:* Evaluates operands (registers, immediates, offsets) and executes the instruction logic.
- **Addressing Modes:**
  - *PC-Relative (`LD`, `ST`):* Target address is `PC + SEXT(offset9)`.
  - *Base + Offset (`LDR`, `STR`):* Target address is `BaseR + SEXT(offset6)`.
  - *Indirect / Pointer (`LDI`, `STI`):* Reads the target address from a pointer location `memory[PC + SEXT(offset9)]`.
  - *Immediate Address (`LEA`):* Loads the computed address directly into a register without accessing memory.
- **Memory-Mapped Input/Output (MMIO):**
  - Reading from address `0xFE00` (`KBSR`) triggers a non-blocking poll of standard input via POSIX `select()`.
  - If a key has been pressed, bit 15 of `KBSR` is set to `1` and the ASCII character is loaded into `0xFE02` (`KBDR`).
- **Terminal Control:**
  - Uses `<termios.h>` to switch standard input to unbuffered raw mode (`~ICANON & ~ECHO`), enabling game loops and character-by-character input without requiring the user to press Enter.
- **Trap Routines (System Calls):**
  - Software traps (`GETC`, `OUT`, `PUTS`, `IN`, `PUTSP`, `HALT`) are intercepted by the host C engine and mapped directly to standard I/O calls for execution.

### 2. The Two-Pass Assembler (`assembler`)
The assembler translates human-readable assembly source files into binary machine code:

- **Pass 1 (Symbol Table Generation):**
  - Strips comments (lines or inline text after `;`) and sanitizes whitespace.
  - Initializes the Location Counter (`LC`) at `.ORIG`.
  - Scans each line to detect labels and stores their names and corresponding addresses in a `SymbolTable`.
  - Detects fatal errors such as duplicate label definitions or illegal labels.
  - Accounts for directives:
    - `.FILL`: Allocates 1 word (`LC += 1`).
    - `.BLKW N`: Allocates $N$ words (`LC += N`).
    - `.STRINGZ "..."`: Allocates length $+ 1$ words (characters + null terminator).

- **Pass 2 (Binary Code Generation):**
  - Re-scans the source file and translates mnemonics into 16-bit binary instructions.
  - Resolves symbolic label references into relative offsets (`PCoffset9`, `PCoffset11`) and verifies range boundaries.
  - Encodes registers (`R0`–`R7`) and immediate values (`#` for decimal, `0x` or `x` for hex).
  - Emits the `.ORIG` starting address as the first 16-bit word, followed by the encoded instruction sequence in Big-Endian format.

---

## Instruction Set Reference

| Opcode | Mnemonic | Assembly Syntax | Operation Description |
| :---: | :--- | :--- | :--- |
| `0001` | **ADD** | `ADD DR, SR1, SR2`<br>`ADD DR, SR1, imm5` | Add registers or immediate value into destination register |
| `0101` | **AND** | `AND DR, SR1, SR2`<br>`AND DR, SR1, imm5` | Bitwise logical AND into destination register |
| `1001` | **NOT** | `NOT DR, SR` | Bitwise inversion (One's complement) |
| `0000` | **BR**  | `BR[nzp] LABEL` | Branch conditionally to label if matching flag is set |
| `1100` | **JMP** | `JMP BaseR` | Unconditional jump to address stored in register |
| `1100` | **RET** | `RET` | Return from subroutine (alias for `JMP R7`) |
| `0100` | **JSR** | `JSR LABEL`<br>`JSRR BaseR` | Jump to subroutine, saving incremented PC into `R7` |
| `0010` | **LD**  | `LD DR, LABEL` | Load word from PC-relative address into register |
| `1010` | **LDI** | `LDI DR, LABEL` | Load word indirect (pointer dereference) into register |
| `0110` | **LDR** | `LDR DR, BaseR, offset6` | Load word from Base register + offset into register |
| `1110` | **LEA** | `LEA DR, LABEL` | Load effective address of label directly into register |
| `0011` | **ST**  | `ST SR, LABEL` | Store word from register to PC-relative address |
| `1011` | **STI** | `STI SR, LABEL` | Store word indirect (pointer dereference) to memory |
| `0111` | **STR** | `STR SR, BaseR, offset6` | Store word from register to Base register + offset |
| `1111` | **TRAP**| `TRAP trapvect8` | Execute system service routine (`GETC`, `OUT`, `PUTS`, etc.) |
| `1000` | **RTI** | `RTI` | Return from interrupt (Supervisor mode privileged instruction) |

---

## Building the Project

The project includes a unified `Makefile` that compiles both executables under standard C99 with POSIX extensions.

### Prerequisites
- A C compiler (`gcc` or `clang`)
- POSIX-compliant environment (Linux, macOS, WSL)
- `make`

### Compilation
To build both the Assembler and the Virtual Machine:
```bash
make
```
To clean all the object files
```bash
make clean
```

### Usage and workflow
First compile the assembler.c and vm.c files
```bash
make
```
Then generate the object file written in LC-3 ISA by using assembler and test.asm
```bash
./assembler test.asm test.obj
```
You will see a symbol table printed on the terminal

Execute the object file using vm
```bash
./vm test.obj
```
You will see a message printed on the terminal

Finally clean all the .obj files
```bash
make clean
```

## References and Acknowledgements

This implementation was developed using the architectural specifications and design guides from:

- Yale N. Patt and Sanjay J. Patel, Introduction to Computing Systems: From Bits and Gates to C and Beyond, McGraw-Hill. (Primary reference for the LC-3 ISA, microarchitecture, and two-pass assembler algorithms).
- Justin Meiners and Ryan Pendleton, Write your Own Virtual Machine, technical tutorial on LC-3 software emulation and terminal control in C.
- LC-3 Architecture Specification (Appendix A & Appendix C), reference specification for instruction bit-encodings, trap routines, and device status registers.

### License
This project is licensed under the MIT License - open for educational and personal use.
