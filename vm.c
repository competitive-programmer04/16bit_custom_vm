#include<stdio.h>
#include<stdint.h>
#include"vm.h"

int running = 1;

uint16_t sign_extend(uint16_t x, uint16_t bit_count){
    if((x>>(bit_count-1))&1){
        x = x|(0xFFFF<<bit_count);
    }
    return x;
}
void set_condition_code(uint16_t reg){
    registers[psr] = registers[psr]&(~0x7);
    if(reg == 0){
        // psr[0] = P, psr[1] = Z, psr[2] = N
        registers[psr] = registers[psr]|0x2;
    }
    else if((reg>>15)&1){
        registers[psr] = registers[psr]|0x4;
    }
    else{
        registers[psr] = registers[psr]|0x1;
    }
    return;
}

void fetch_decode_execute(){
    // fetching the instruction
    uint16_t ins = memory[registers[pc]++];
    uint16_t opcode = ins>>12;
    // decoding the instruction
    switch(opcode){
        // executing the instruction
        case OP_ILL:{
            running = 0; // aborting the program
            break;
        }
        case OP_BR:{
            uint16_t pc_offset_9 = ins&0x1FF;
            // uint16_t pos = (ins>>9)&0x1;
            // uint16_t zero = (ins>>9)&0x2;
            // uint16_t neg = (ins>>9)&0x4;
            uint16_t cond_flag = (ins>>9)&0x7;
            if(cond_flag&(registers[psr]&0x7)){
                registers[pc] = registers[pc] + sign_extend(pc_offset_9, 9);
            }
            break;
        }
        case OP_ADD:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t sr1 = (ins>>6)&0x7;
            if((ins>>5)&1){
                // immediate mode
                uint16_t imm5 = sign_extend(ins&0x1F,5);
                registers[dr] = registers[sr1] + imm5;
            }
            else{
                uint16_t sr2 = ins&0x7;
                registers[dr] = registers[sr1] + registers[sr2];
            }
            set_condition_code(registers[dr]);
            break;
        }
        case OP_AND:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t sr1 = (ins>>6)&0x7;
            if((ins>>5)&1){
                uint16_t imm5 = sign_extend(ins&0x1F, 5);
                registers[dr] = registers[sr1]&imm5;
            }
            else{
                uint16_t sr2 = ins&0x7;
                registers[dr] = registers[sr1] & registers[sr2];
            }
            set_condition_code(registers[dr]);
            break;
        }
        case OP_NOT:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t sr = (ins>>6)&0x7;
            registers[dr] = ~registers[sr];
            set_condition_code(registers[dr]);
            break;
        }
        case OP_JSR:{
            registers[r7] = registers[pc];
            if((ins>>11)&1){
                //jsr
                uint16_t pc_offset_11 = sign_extend(ins&0x7FF, 11);
                registers[pc] = registers[pc] + pc_offset_11;
            }
            else{
                //jsrr
                uint16_t base_r = (ins>>6)&0x7;
                registers[pc] = registers[base_r];
            }
            break;
        }
        case OP_JMP:{
            uint16_t base_r = (ins>>6)&0x7;
            if(base_r == 7){
                //ret;
                registers[pc] = registers[r7];
            }
            else{
                //jmp
                registers[pc] = registers[base_r];
            }
            break;
        }
        case OP_LD:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            registers[dr] = memory[registers[pc] + pc_offset_9];
            set_condition_code(registers[dr]);
            break;
        }
        case OP_LDI:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            registers[dr] = memory[memory[registers[pc] + pc_offset_9]];
            set_condition_code(registers[dr]);
            break;
        }
        case OP_LDR:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t base_r = (ins>>6)&0x7;
            uint16_t pc_offset_6 = sign_extend(ins&0x3F,6);
            registers[dr] = memory[registers[base_r] + pc_offset_6];
            set_condition_code(registers[dr]);
            break;
        }
        case OP_LEA:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            registers[dr] = registers[pc] + pc_offset_9;
            set_condition_code(registers[dr]);
            break;
        }
        case OP_ST:{
            uint16_t sr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            memory[registers[pc] + pc_offset_9] = registers[sr];
            break;
        }
        case OP_STI:{
            uint16_t sr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            memory[memory[registers[pc]+pc_offset_9]] = registers[sr];
            break;
        }
        case OP_STR:{
            uint16_t sr = (ins>>9)&0x7;
            uint16_t base_r = (ins>>6)&0x7;
            uint16_t pc_offset_6 = sign_extend(ins&0x3F, 6);
            memory[registers[base_r]+pc_offset_6] = registers[sr];
            break;
        }
        case OP_RTI:
            //TODO
            break;
        case OP_TRAP:
            running = 0;
            break;
    }
    return;
}

int main(void){
    for(size_t i=0; i<NUM_REG; ++i){
        registers[i] = 0;
    }
    for(size_t i=0; i<NUM_ADDR; ++i){
        memory[i] = 0;
    }
    enum{
        PC_START = 0x3000
    }
    registers[pc] = PC_START; // program will start from memory address 0x3000
                              // because the user space will be starting from memory address
                              // 0x3000
    while(running){
        fetch_decode_execute();
    }
    return 0;
}
