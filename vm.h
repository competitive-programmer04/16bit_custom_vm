#ifndef __VM__H__
#define __VM_H__
#define NUM_REG 10
#define NUM_ADDR (1<<16)

// defining opcodes -> 4bits
enum{
    OP_BR = 0, // branch
    OP_ADD = 1, // add
    OP_LD = 2, // load direct
    OP_ST = 3, // store direct
    OP_JSR = 4, 
    OP_AND = 5, // and 
    OP_LDR = 6, // load direct + offset
    OP_STR = 7, // store direct + offset
    OP_RTI = 8, // privleged instruction
    OP_NOT = 9, // not
    OP_LDI = 10, // load indirect
    OP_STI = 11, // store indirect
    OP_JMP = 12,
    OP_ILL = 13, // illegal opcode
    OP_LEA = 14, // load effective address
    OP_TRAP = 15 // trap
};

// defining registers
enum{
    r0 = 0,
    r1,
    r2,
    r3,
    r4,
    r5,
    r6, // stack pointer,
    r7,
    pc, // program counter
    psr, // processor status register
};

uint16_t registers[NUM_REG];
uint16_t memory[NUM_ADDR]; // word addressable i.e. each memory address will be pointing to 16 bit word
uint16_t sign_extend(uint16_t x, uint16_t bit_count);
void set_condition_code(uint16_t reg);
void fetch_decode_execute();
#endif

