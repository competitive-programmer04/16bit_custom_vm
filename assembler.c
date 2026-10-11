#include <endian.h>
#include <iso646.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include <sys/types.h>
#include"assembler.h"
#define MAX_LINE_SIZE 512

uint16_t addr_counter = 0;

const char *my_string[] =  {
        "ADD", "AND", "NOT", "BR", "BRN", "BRZ", "BRP", "BRNZ", "BRNP", "BRZP", "BRNZP",
        "JMP", "RET", "JSR", "JSRR", "LD", "LDI", "LDR", "LEA", "ST", "STI", "STR",
        "TRAP", "GETC", "OUT", "PUTS", "IN", "PUTSP", "HALT", "RTI",
        ".ORIG", ".FILL", ".BLKW", ".STRINGZ", ".END",
    };
    int my_string_size = 35;

    const char* my_registers[] = {"r0", "R0", "r1", "R1", "r2", "R2", "r3", "R3", "r4", "R4", "r5", "R5", "r6", "R6", "r7", "R7"};

    enum {
        OP_BR = 0x0,
        OP_ADD = 0x1,
        OP_LD = 0x2,
        OP_ST = 0x3,
        OP_JSR = 0x4,
        OP_JSRR = 0x4,
        OP_AND = 0x5,
        OP_LDR = 0x6,
        OP_STR = 0x7,
        OP_RTI = 0x8,
        OP_NOT = 0x9,
        OP_LDI = 0xA,
        OP_STI = 0xB,
        OP_JMP = 0xC,
        OP_RET = 0xC,
        OP_ILL = 0xD,
        OP_LEA = 0xE,
        OP_TRAP = 0xF
    };

uint16_t parse_reg(char *str){
    if(!str || *str == '\0'){
        fprintf(stderr, "Pass the register in correct format\n");
        return 8;
    }
    else if(strlen(str) != 2){
        fprintf(stderr, "available general purpose registers are r0/R0, r1/R1 ..... r7/R7. Don't use any other\n");
        return 8;
    }
    uint16_t reg_num = str[1] - '0';
    return reg_num;
}

int16_t parse_imm(char *str){
    if(!str || *str == '\0') return 0;
    while(*str == ' ' || *str == '\t'){
        str++;
    }
    char *endptr;
    long val;
    // for decimal like #10
    if(*str == '#'){
        val = strtol(str+1, &endptr, 10);
    }
    // for hexadecimal like 0x0016 or 0X0025
    else if(strncmp(str, "0x",2) == 0 || strncmp(str, "0X", 2) == 0){
        val = strtol(str, &endptr, 16);
    }
    // also for hexadecimal
    else if(strncmp(str, "x", 1) == 0 || strncmp(str, "X", 1) == 0){
        val = strtol(str, &endptr, 16);
    }
    // for decimal
    else{
        val = strtol(str, &endptr, 10);
    }
    if(str == endptr){
        fprintf(stderr, "some error occurred in parsing. Cannot parse the value to integer\n");
        return -1;
    }
    if(val > INT16_MAX){
        fprintf(stderr, "maximum allowed bits are 16. No element can have more than 16 bits\n");
        return -1;
    }
    int16_t out_val = (int16_t)val;
    return out_val;
}

uint16_t encode_instruction(const char *opcode, char *args, uint16_t current_pc, SymbolTable *st, uint16_t line_num){
    // 1 Branch Instruction
    if(strstr(opcode, "BR") != NULL){
        uint16_t neg = 0;
        uint16_t zero = 0;
        uint16_t pos = 0;
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ' ' && *args != '\n' && *args != '\t' && *args != '\0'){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass a label with branch instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. label not found\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc+1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 and 255\n", line_num);
            return 0xFFFF;
        }
        if(strcmp(opcode, "BRn") == 0 || strcmp(opcode, "BRnz") == 0 || strcmp(opcode, "BRnp") == 0 || strcmp(opcode, "BRnzp") == 0){
            neg = 0x1<<11;
        }
        if(strcmp(opcode, "BRz") == 0 || strcmp(opcode, "BRzp") == 0 || strcmp(opcode, "BRnz") == 0 || strcmp(opcode, "BRnzp") == 0){
            zero = 0x1<<10;
        }
        if(strcmp(opcode, "BRp") == 0 || strcmp(opcode, "BRnp") == 0 || strcmp(opcode, "BRzp") == 0 || strcmp(opcode, "BRnzp") == 0){
            pos = 0x1<<9;
        }
        uint16_t ins = (OP_BR<<12)|neg|zero|pos|(pcoffset9 & 0x1FF);
        return ins;
    }
    // 2 ADD Instruction
    else if(strcmp(opcode, "ADD") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != ' ' && *args != '\n' && *args != '\t' && *args != '\0'){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass destination register.\n", line_num);
            return 0xFFFF;
        }
        char my_args[name_len+1];
        memset(my_args, 0, name_len);
        memcpy(my_args, old_ptr, name_len);
        my_args[name_len] = '\0';
        uint16_t des = parse_reg(my_args);
        if(des >= 8){
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        name_len = 0;
        old_ptr = args;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass first source register\n", line_num);
            return 0xFFFF;
        }
        memcpy(my_args, old_ptr, name_len);
        uint16_t src1 = parse_reg(my_args);
        if(src1 >= 8){
            return 0xFFFF;
        }
        while(*args == ' ' || *args != ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != ' ' && *args != '\n' && *args != '\t' && *args != '\0'){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass third operand\n", line_num);
            return 0xFFFF;
        }
        char my_operands[name_len+1];
        memset(my_operands, 0, name_len);
        memcpy(my_operands, old_ptr, name_len);
        my_operands[name_len] = '\0';
        int is_register = 0;
        for(int i=0; i<16; ++i){
            if(strcmp(my_operands, my_registers[i]) == 0){
                is_register = 1;
                break;
            }
        }
        if(is_register){
            uint16_t src2 = parse_reg(my_operands);
            if(src2 >= 8){
                return 0xFFFF;
            }
            uint16_t ins = (OP_ADD<<12)|((des&0x7)<<9)|((src1&0x7)<<6)|(src2&0x7);
            return ins;
        }
        else{
            int16_t imm5 = parse_imm(my_operands);
            if(imm5 > 15 || imm5 < -16){
                fprintf(stderr, "error on line number: %d. immediate value goes out of range\n", line_num);
                return 0xFFFF;
            }
            uint16_t ins = (OP_ADD<<12)|((des&0x7)<<9)|((src1&0x7)<<6)|(0x1<<5)|(imm5&0x1F);
            return ins;
        }
    }
    // 3 AND Instruction
    else if(strcmp(opcode, "AND") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass destination register\n", line_num);
            return 0xFFFF;
        }
        char my_args[name_len+1];
        memset(my_args, 0, name_len);
        memcpy(my_args, old_ptr, name_len);
        my_args[name_len] = '\0';
        uint16_t des = parse_reg(my_args);
        if(des >= 8){
            fprintf(stderr, "error on line number: %d. Please pass the destination register\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args != ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the first source register\n", line_num);
            return 0xFFFF;
        }
        memcpy(my_args, old_ptr, name_len);
        uint16_t src1 = parse_reg(my_args);
        if(src1 >= 8){
            fprintf(stderr, "error on line number: %d. Please pass the source register one in correct format\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the third operand\n", line_num);
            return 0xFFFF;
        }
        char my_operands[name_len+1];
        memset(my_operands, 0, name_len);
        memcpy(my_operands, old_ptr, name_len);
        my_operands[name_len] = '\0';
        int is_register = 0;
        for(int i=0; i<16; ++i){
            if(strcmp(my_operands, my_registers[i]) == 0){
                is_register = 1;
                break;
            }
        }
        if(is_register){
            uint16_t src2 = parse_reg(my_operands);
            if(src2 >= 8){
                fprintf(stderr, "error on line number: %d. Please pass the third operand as a register\n", line_num);
                return 0xFFFF;
            }
            uint16_t ins = (OP_AND<<12)|((des&0x7)<<9)|((src1&0x7)<<6)|(src2&0x7);
            return ins;
        }
        else{
            int16_t imm5 = parse_imm(my_operands);
            if(imm5 > 15 || imm5 < -16){
                fprintf(stderr, "error on line number: %d. Immediate value overflow\n", line_num);
                return 0xFFFF;
            }
            uint16_t ins = (OP_AND<<12)|((des&0x7)<<9)|((src1&0x7)<<6)|(0x1<<5)|(imm5&0x1F);
            return ins;
        }
    }
    // 4 NOT Instruction
    else if(strcmp(opcode, "NOT") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the destination register\n", line_num);
            return 0xFFFF;
        }
        char my_args[name_len+1];
        memset(my_args, 0, name_len);
        memcpy(my_args, old_ptr, name_len);
        my_args[name_len] = '\0';
        uint16_t des = parse_reg(my_args);
        if(des >= 8){
            fprintf(stderr, "error on line number: %d. Pass the destination register in correct format\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args != ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Pass the source register\n", line_num);
            return 0xFFFF;
        }
        memcpy(my_args, old_ptr, name_len);
        my_args[name_len+1] = '\0';
        uint16_t src = parse_reg(my_args);
        if( src >= 8){
            fprintf(stderr, "error on line number: %d. Pass the source register in correct way\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_NOT<<12)|((des&0x7)<<9)|((src&0x7)<<6);
        return ins;
    }
    // 5 JSR Instruction
    else if(strcmp(opcode, "JSR") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass a label after JSR instruction\n", line_num);
            return 0xFFFF;
        }
        char name[name_len+1];
        memset(name, 0, name_len);
        memcpy(name, old_ptr, name_len);
        name[name_len+1] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(name, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label not found in the symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset11 = addr - (current_pc + 1);
        if(pcoffset11>1023 || pcoffset11 < -1024){
            fprintf(stderr, "errorn on line number: %d. Offset should be in the range of -1024 to 1023\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_JSR<<12)|(0x1<<11)|(pcoffset11&0x7FF);
        return ins;
    }
    // 6 JSRR Instruction
    else if(strcmp(opcode, "JSRR") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t base_r = parse_reg(reg_name);
        if(base_r >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot excced 7\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_JSRR<<12)|((base_r&0x7)<<6);
        return ins;
    }
    // 7 JMP Instruction
   else if(strcmp(opcode, "JMP") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t base_r = parse_reg(reg_name);
        if(base_r >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot excced 7\n", line_num);
            return 0xFFFF;
        }
        else if(base_r == 7){
            fprintf(stderr, "error on line number. %d. Base register cannot be 7 in JMP instruction\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_JMP<<12)|((base_r&0x7)<<6);
        return ins;
    }
   // 8 RET Instruction
   else if(strcmp(opcode, "RET") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t base_r = parse_reg(reg_name);
        if(base_r >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot excced 7\n", line_num);
            return 0xFFFF;
        }
        else if(base_r != 7){
            fprintf(stderr, "error on line number. %d. Base register should be 7 in ret instruction\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_JMP<<12)|((base_r&0x7)<<6);
        return ins;
    }
   // 9 LOAD Instruction
    else if(strcmp(opcode, "LD") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t des = parse_reg(reg_name);
        if(des >= 8){
            fprintf(stderr, "errorn line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing Label in LOAD Instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label does not match with any in the symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc + 1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 to 255\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_LD<<12)|((des&0x7)<<9)|(pcoffset9&0x1FF);
        return ins;
    }
    // 10 LOAD INDIRECT Instruction
   else if(strcmp(opcode, "LDI") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t des = parse_reg(reg_name);
        if(des >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing Label in LOAD Indirect Instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label doen not match with any in symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc + 1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 to 255\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_LDI<<12)|((des&0x7)<<9)|(pcoffset9&0x1FF);
        return ins;
    }
   // 11 LDR Instruction
   else if(strcmp(opcode, "LDR") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t des = parse_reg(reg_name);
        if(des >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing the base register\n", line_num);
            return 0xFFFF;
        }
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t base_r = parse_reg(reg_name);
        if(base_r >= 8){
            fprintf(stderr, "errorn line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing label in LDR instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label does not match with any in symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset6 = addr - (current_pc + 1);
        if(pcoffset6 > 31 || pcoffset6 < -32){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -32 to 31\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_LDR<<12)|((des&0x7)<<9)|((base_r&0x7)<<6)|(pcoffset6&0x3F);
        return ins;
    }
   // 12 STORE Instruction
   else if(strcmp(opcode, "ST") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t src = parse_reg(reg_name);
        if(src >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing label in STORE Instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label does not match with any in symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc + 1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 to 255\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_ST<<12)|((src&0x7)<<9)|(pcoffset9&0x1FF);
        return ins;
    }
   // 13 STORE INDIRECT Instruction
    else if(strcmp(opcode, "STI") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t src = parse_reg(reg_name);
        if(src >= 8){
            fprintf(stderr, "errorn line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing Label in STORE Indirect Instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label doen not match with any in symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc + 1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 to 255\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_STI<<12)|((src&0x7)<<9)|(pcoffset9&0x1FF);
        return ins;
    }
    // 14 STR Instruction
   else if(strcmp(opcode, "STR") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t src = parse_reg(reg_name);
        if(src >= 8){
            fprintf(stderr, "errorn line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing the base register\n", line_num);
            return 0xFFFF;
        }
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t base_r = parse_reg(reg_name);
        if(base_r >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing label in STR instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label does not match with any in symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset6 = addr - (current_pc + 1);
        if(pcoffset6 > 31 || pcoffset6 < -32){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -32 to 31\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_STR<<12)|((src&0x7)<<9)|((base_r&0x7)<<6)|(pcoffset6&0x3F);
        return ins;
    }
   // 15 LEA Instruction
    else if(strcmp(opcode, "LEA") == 0){
        char *old_ptr = args;
        int name_len = 0;
        while(*args != ',' && *args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Please pass the register\n", line_num);
            return 0xFFFF;
        }
        char reg_name[name_len+1];
        memset(reg_name, 0, name_len);
        memcpy(reg_name, old_ptr, name_len);
        reg_name[name_len] = '\0';
        uint16_t des = parse_reg(reg_name);
        if(des >= 8){
            fprintf(stderr, "error on line number: %d. Register number cannot exceed 7\n", line_num);
            return 0xFFFF;
        }
        while(*args == ' ' || *args == ','){
            args++;
        }
        old_ptr = args;
        name_len = 0;
        while(*args != '\n' && *args != '\t' && *args != '\0' && *args != ' '){
            name_len++;
            args++;
        }
        if(name_len == 0){
            fprintf(stderr, "error on line number: %d. Missing Label in LEA Instruction\n", line_num);
            return 0xFFFF;
        }
        char my_label[name_len+1];
        memset(my_label, 0, name_len);
        memcpy(my_label, old_ptr, name_len);
        my_label[name_len] = '\0';
        uint16_t addr = 0;
        for(int i=0; i<st->count; ++i){
            if(strcmp(my_label, st->my_symbol[i].label_name) == 0){
                addr = st->my_symbol[i].label_addr;
                break;
            }
        }
        if(addr == 0){
            fprintf(stderr, "error on line number: %d. Label does not match with any in the symbol table\n", line_num);
            return 0xFFFF;
        }
        int16_t pcoffset9 = addr - (current_pc + 1);
        if(pcoffset9 > 255 || pcoffset9 < -256){
            fprintf(stderr, "error on line number: %d. Offset should be in the range of -256 to 255\n", line_num);
            return 0xFFFF;
        }
        uint16_t ins = (OP_LEA<<12)|((des&0x7)<<9)|(pcoffset9&0x1FF);
        return ins;
    }
    // 16 RTI Instruction
    else if(strcmp(opcode, "RTI") == 0){
        uint16_t ins = (OP_RTI<<12);
        return ins;
    }
    // 17 TRAP Instruction
    else if(strcmp(opcode, "GETC") == 0){
        return 0xF020;
    }
    else if(strcmp(opcode, "OUT") == 0){
        return 0xF021;
    }
    else if(strcmp(opcode, "PUTS") == 0){
        return 0xF022;
    }
    else if(strcmp(opcode, "IN") == 0){
        return 0xF023;
    }
    else if(strcmp(opcode, "PUTSP") == 0){
        return 0xF024;
    }
    else if(strcmp(opcode, "HLT") == 0){
        return 0xF025;
    }
    else{
        fprintf(stderr, "error on line number: %d. Invalid opcode\n", line_num);
        return 0xFFFF;
    }
 }

void init_symbol_table(SymbolTable *st){
    memset(st->my_symbol, 0, sizeof(st->my_symbol));
    st->count = 0;
    return;
}

int check_symbol(SymbolTable *st, char *name){
    for(int i=0; i<st->count; ++i){
        if(strcmp(name, st->my_symbol[i].label_name)==0){
            return -1;
        }
    }
    return 0;
}

int add_symbol(SymbolTable *st, char *name, uint16_t addr){
    if(check_symbol(st, name) == -1){
        return -1;
    }
    else if(strlen(name) > MAX_LABEL_LEN){
        return -2;
    }
    else if(st->count > MAX_LABELS){
        return -3;
    }
    memcpy(st->my_symbol[st->count].label_name, name, strlen(name));
    st->my_symbol[st->count].label_name[strlen(name)] = '\0';
    st->my_symbol[st->count].label_addr = addr;
    st->count++;
    return 0;
}

int pass2(SymbolTable *st, FILE *fp_r, FILE *fp_w){
    addr_counter = 0;
    uint16_t line_num = 0;
    char line_buf[MAX_LINE_SIZE+1];
    memset(line_buf, 0, MAX_LINE_SIZE);
    while(1){
        if(fgets(line_buf, sizeof(line_buf), fp_r) == NULL){
            if(feof(fp_r)){
                printf("we have reached the end of the assembly file\n");
                break;
            }
            else if(ferror(fp_r)){
                printf("some error occurred while reading instructions from the assembly file\n");
                return -1;
            }
        }
        else{
            char *comments = strchr(line_buf, ';');
            if(comments) *comments = '\0';
            char *ptr = line_buf;
            while(*ptr == ' '){
                ptr++;
            }
            if(*ptr == '\0') continue; // commented line
            char *old_ptr = ptr;
            uint16_t name_len = 0;
            while(*ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\0'){
                name_len++;
                ptr++;
            }
            if(name_len == 0) continue;
            char name[name_len+1];
            memset(name, 0, name_len);
            memcpy(name, old_ptr, name_len);
            name[name_len] = '\0';
            int is_check = 0;
            for(int i =0; i<my_string_size; ++i){
                if(strcmp(name, my_string[i]) == 0){
                    is_check = 1;
                    break;
                }
            }
            if(!is_check){
                while(*ptr == ' '){
                    ptr++;
                }
                old_ptr = ptr;
                name_len = 0;
                while(*ptr != ' ' && *ptr != '\n' && *ptr != '\t' && *ptr != '\0'){
                    name_len++;
                    ptr++;
                }
                if(name_len == 0){
                    fprintf(stderr, "error on line number: %d. pass an opcode or a pseudo directive after label\n", line_num);
                    return -1;
                }
                char new_name[name_len+1];
                memset(new_name, 0, name_len);
                memcpy(new_name, old_ptr, name_len);
                new_name[name_len] = '\0';
                if(strcmp(new_name, ".ORIG") == 0){
                    fprintf(stderr, "error on line number: %d. You cannot use .ORIG after the label in a file\n", line_num);
                    return -1;
                }
                else if(strcmp(new_name, ".FILL") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    old_ptr = ptr;
                    name_len = 0;
                    while(*ptr != ' ' && *ptr != '\0' && *ptr != '\t' && *ptr != '\n'){
                        name_len++;
                        ptr++;
                    }
                    if(name_len == 0){
                        fprintf(stderr, "error on line number: %d. pass a number or a Label after fill\n", line_num);
                        return -1;
                    }
                    char my_value[name_len+1];
                    memset(my_value, 0, name_len);
                    memcpy(my_value, old_ptr, name_len);
                    my_value[name_len] = '\0';
                    int is_label = 0;
                    for(int i=0; i<st->count; ++i){
                        if(strcmp(my_value, st->my_symbol[i].label_name) == 0){
                            is_label = 1;
                            fwrite(&st->my_symbol[i].label_addr, sizeof(uint16_t), 1, fp_w);
                            break;
                        }
                    }
                    if(!is_label){
                        int16_t val = parse_imm(name);
                        fwrite(&val, sizeof(int16_t), 1, fp_w);
                    }
                    addr_counter = addr_counter + 1;
                }
                else if(strcmp(new_name, ".BLKW") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    old_ptr = ptr;
                    name_len = 0;
                    while(*ptr != ' ' && *ptr != '\n' && *ptr != '\t' && *ptr != '\0'){
                        name_len++;
                        ptr++;
                    }
                    if(name_len == 0){
                        fprintf(stderr, "error on line number: %d. Please pass a number\n", line_num);
                        return -1;
                    }
                    char num_words[name_len+1];
                    memset(num_words, 0, name_len);
                    memcpy(num_words, old_ptr, name_len);
                    num_words[name_len] = '\0';
                    int16_t val = parse_imm(num_words);
                    for(int i=1; i<=val; ++i){
                        uint16_t my_num = 0x0000;
                        fwrite(&my_num, sizeof(uint16_t), 1, fp_w);
                    }
                    addr_counter = addr_counter + val;
                }
                else if(strcmp(new_name, ".STRINGZ") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    char *start_quote = strchr(ptr, '\"');
                    if(start_quote){
                        char *end_quote = strchr(start_quote, '\"');
                        ptr = start_quote + 1;
                        while(ptr<end_quote){
                            uint16_t val = *ptr - '0';
                            fwrite(&val, sizeof(uint16_t), 1, fp_w);
                            ptr++;
                        }
                        uint16_t null_terminator = 0x0000;
                        fwrite(&null_terminator, sizeof(uint16_t), 1, fp_w);
                        addr_counter = addr_counter + (end_quote-start_quote+1) + 1;
                    }
                }
                else if(strcmp(new_name, ".END") == 0){
                    printf("we have reached at the end of the assembly program\n");
                    break;
                }
                else{
                    while(*ptr == ' '){
                        ptr++;
                    }
                    uint16_t ins = encode_instruction(new_name, ptr, addr_counter, st, line_num);
                    if(ins == 0xFFFF){
                        return -1;
                    }
                    fwrite(&ins, sizeof(uint16_t), 1, fp_w);
                    addr_counter = addr_counter + 1;
                }
            }
            else{
                if(strcmp(name, ".ORIG") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    old_ptr = ptr;
                    name_len = 0;
                    while(*ptr != '\n' && *ptr != '\t' && *ptr != '\0' && *ptr != ' '){
                        name_len++;
                        ptr++;
                    }
                    if(name_len == 0){
                        fprintf(stderr, "error on line number: %d.Address missing after .ORIG\n", line_num);
                        return -1;
                    }
                    char my_addr[name_len+1];
                    memset(my_addr, 0, name_len);
                    memcpy(my_addr, old_ptr, name_len);
                    my_addr[name_len] = '\0';
                    unsigned long addr = strtoul(my_addr, NULL, 16);
                    if(addr > UINT16_MAX){
                        fprintf(stderr, "error on line number: %d. Address can not be greater than 16 bits\n", line_num);
                        return -1;
                    }
                    uint16_t orig_addr = (uint16_t)addr;
                    fwrite(&orig_addr, sizeof(uint16_t), 1, fp_w);
                    addr_counter = orig_addr;
                }
                else if(strcmp(name, ".FILL") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    old_ptr = ptr;
                    name_len = 0;
                    while(*ptr != ' ' && *ptr != '\0' && *ptr != '\t' && *ptr != '\n'){
                        name_len++;
                        ptr++;
                    }
                    if(name_len == 0){
                        fprintf(stderr, "error on line number: %d. pass a number or a Label after fill\n", line_num);
                        return -1;
                    }
                    char my_value[name_len+1];
                    memset(my_value, 0, name_len);
                    memcpy(my_value, old_ptr, name_len);
                    my_value[name_len] = '\0';
                    int is_label = 0;
                    for(int i=0; i<st->count; ++i){
                        if(strcmp(my_value, st->my_symbol[i].label_name) == 0){
                            is_label = 1;
                            fwrite(&st->my_symbol[i].label_addr, sizeof(uint16_t), 1, fp_w);
                            break;
                        }
                    }
                    if(!is_label){
                        int16_t val = parse_imm(name);
                        fwrite(&val, sizeof(int16_t), 1, fp_w);
                    }
                    addr_counter = addr_counter + 1;
                }
                else if(strcmp(name, ".BLKW") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    old_ptr = ptr;
                    name_len = 0;
                    while(*ptr != ' ' && *ptr != '\n' && *ptr != '\t' && *ptr != '\0'){
                        name_len++;
                        ptr++;
                    }
                    if(name_len == 0){
                        fprintf(stderr, "error on line number: %d. Please pass a number\n", line_num);
                        return -1;
                    }
                    char num_words[name_len+1];
                    memset(num_words, 0, name_len);
                    memcpy(num_words, old_ptr, name_len);
                    num_words[name_len] = '\0';
                    int16_t val = parse_imm(num_words);
                    for(int i=1; i<=val; ++i){
                        uint16_t my_num = 0x0000;
                        fwrite(&my_num, sizeof(uint16_t), 1, fp_w);
                    }
                    addr_counter = addr_counter + val;
                }
                else if(strcmp(name, ".STRINGZ") == 0){
                    while(*ptr == ' '){
                        ptr++;
                    }
                    char *start_quote = strchr(ptr, '\"');
                    if(start_quote){
                        char *end_quote = strchr(start_quote, '\"');
                        ptr = start_quote + 1;
                        while(ptr<end_quote){
                            uint16_t val = *ptr - '0';
                            fwrite(&val, sizeof(uint16_t), 1, fp_w);
                            ptr++;
                        }
                        uint16_t null_terminator = 0x0000;
                        fwrite(&null_terminator, sizeof(uint16_t), 1, fp_w);
                        addr_counter = addr_counter + (end_quote-start_quote+1) + 1;
                    }
                }
                else if(strcmp(name, ".END") == 0){
                    printf("we have reached at the end of the assembly program\n");
                    break;
                }
                else{
                    while(*ptr == ' '){
                        ptr++;
                    }
                    uint16_t ins = encode_instruction(name, ptr, addr_counter, st, line_num);
                    if(ins == 0xFFFF){
                        return -1;
                    }
                    fwrite(&ins, sizeof(uint16_t), 1, fp_w);
                    addr_counter = addr_counter + 1;
                }
            }
        }
        line_num++;
    }
    return 0;
}

int pass1(SymbolTable *st, FILE *fp){
    uint16_t line_num = 1;
    char line_buf[MAX_LINE_SIZE+1];
    memset(line_buf, 0, MAX_LINE_SIZE);
    while(1){
       if(fgets(line_buf, sizeof(line_buf), fp)==NULL){
           if(feof(fp)){
               printf("we reached the end of the assembly file\n");
               break;
           }
           else if(ferror(fp)){
               printf("some error occured in assembling\n");
               return -1;
           }
       }
       else{
        char *comments = strchr(line_buf, ';');
        if(comments) *comments = '\0';
           char *ptr = line_buf;
           // trimming of the trailing white spaces
           while(*ptr == ' '){
               ptr++;
           }
           if(*ptr == '\0') continue; // commented line
           char *old_ptr = ptr;
           uint16_t name_len = 0;
           while(*ptr != ' '&& *ptr != '\n' && *ptr != '\t' && *ptr != '\0'){
               name_len++;
               ptr++;
           }
           if(name_len == 0) continue;
           char name[name_len+1];
           memset(name, 0, name_len);
           memcpy(name, old_ptr, name_len);
           name[name_len] = '\0';
           int is_find = 0;
           for(int i=0; i<my_string_size; ++i){
            if(strcmp(name, my_string[i])==0){
                is_find = 1;
                break;
           }
        }
           if(!is_find){
               int result = add_symbol(st, name, addr_counter);
               if(result == -1){
                   printf("error on line number: %d. Label name already found\n", line_num);
                   return -1;
               }
               else if(result == -2){
                   printf("error on line number: %d. Label name cannot be greater than 24 characters\n", line_num);
                   return -1;
               }
               else if(result == -3){
                   printf("error on line number: %d. Total number of labels cannot exceed 1024\n", line_num);
                   return -1;
               }
               else{
                   while(*ptr == ' '){
                       ptr++;
                   }
                   old_ptr = ptr;
                   name_len = 0;
                   while(*ptr != ' ' && *ptr != '\n' && *ptr != '\t' && *ptr != '\0'){
                       name_len++;
                       ptr++;
                   }
                   if(name_len == 0){
                       fprintf(stderr, "error on line number: %d. Please pass an opcode or a pseudo directive after label\n", line_num);
                       return -1;
                   }
                   char new_name[name_len+1];
                   memset(new_name, 0, name_len);
                   memcpy(new_name, old_ptr, name_len);
                   new_name[name_len] = '\0';
                   if(strcmp(new_name, ".ORIG")==0){
                       printf("error on line number: %d. Two origins can not be use in the same file\n", line_num);
                       return -1;
                   }
                   else if(strcmp(new_name, ".FILL")==0){
                       addr_counter = addr_counter + 1;
                   }
                   else if(strcmp(new_name, ".BLKW")==0){
                       while(*ptr == ' '){
                           ptr++;
                       }
                       old_ptr = ptr;
                       name_len = 0;
                       while(*ptr != '\n' && *ptr != ' ' && *ptr != '\t' && *ptr != '\0'){
                           name_len++;
                           ptr++;
                       }
                       if(name_len == 0){
                           fprintf(stderr, "error on line number: %d. Missing value after .BLKW\n", line_num);
                           return -1;
                       }
                       char num_words[name_len+1];
                       memset(num_words, 0, name_len);
                       memcpy(num_words, old_ptr, name_len);
                       num_words[name_len] = '\0';
                       uint16_t offset = 0;
                       for(int i=0; i<name_len; ++i){
                        if(num_words[i] == '#') continue;
                           int mul = 1;
                           for(int j = 0; j<name_len-1-i; ++j){
                               mul = mul*10;
                           }
                           offset = offset + ((num_words[i]-'0')*mul);
                       }
                       addr_counter = addr_counter + offset;
                   }
                   else if(strcmp(new_name, ".STRINGZ")==0){
                       while(*ptr == ' '){
                           ptr++;
                       }
                       char *start_quote = strchr(ptr, '\"');
                       if(start_quote){
                        char *end_quote = strchr(start_quote+1, '\"');
                        name_len = end_quote - start_quote - 1;
                        addr_counter = addr_counter + name_len + 1;
                       }
                   }
                   else if(strcmp(new_name, ".END")==0){
                       printf("We have reached the end of the assembly file\n");
                       break;
                   }
                   else{
                       printf("We have encountered an opcode\n");
                       addr_counter = addr_counter + 1;
                   }
               }
            }
           else{
               if(strcmp(name, ".ORIG")==0){
                   static int cnt = 0;
                   if(cnt >= 1){
                       printf("error on line number: %d. You can not use origin more than 1 time in ya single file.\n", line_num);
                       return -1;
                   }
                   while(*ptr == ' '){
                       ptr++;
                   }
                   old_ptr = ptr;
                   name_len = 0;
                   while(*ptr != '\n' && *ptr != ' ' && *ptr != '\t' && *ptr != '\0'){
                       name_len++;
                       ptr++;
                   }
                   char orig_addr[name_len+1];
                   memset(orig_addr, 0, name_len);
                   memcpy(orig_addr, old_ptr, name_len);
                   orig_addr[name_len] = '\0';
                   unsigned long addr = strtoul(orig_addr, NULL, 16);
                   if(addr > UINT16_MAX){
                       printf("error on line number: %d. Memory address cannot be greater than 16 bits.\n", line_num);
                       return -1;
                   }
                   addr_counter = (uint16_t)addr;
                   cnt++;
               }
               else if(strcmp(name, ".FILL")==0){
                   addr_counter = addr_counter + 1;
               }
               else if(strcmp(name, ".BLKW")==0){
                   while(*ptr == ' '){
                       ptr++;
                   }
                   old_ptr = ptr;
                   name_len = 0;
                   while(*ptr != '\n' && *ptr != ' ' && *ptr != '\t' && *ptr != '\0'){
                       name_len++;
                       ptr++;
                   }
                   if(name_len == 0){
                       fprintf(stderr, "error on line number: %d. Missing number after .FILL\n", line_num);
                       return -1;
                   }
                   char my_offset[name_len+1];
                   memset(my_offset, 0, name_len);
                   memcpy(my_offset, old_ptr, name_len);
                   my_offset[name_len] = '\0';
                   uint16_t offset = 0;
                   for(int i = 0; i<name_len; ++i){
                    if(my_offset[i] == '#') continue;
                       int mul = 1;
                       for(int j=0; j<name_len-1-i; ++j){
                           mul = mul*10;
                       }
                       offset = offset + ((my_offset[i]-'0')*mul);
                   }
                   addr_counter = addr_counter + offset;
               }
               else if(strcmp(name, ".STRINGZ")==0){
                   while(*ptr == ' '){
                       ptr++;
                   }
                   name_len = 0;
                   char *start_quote = strchr(ptr, '\"');
                   if(start_quote){
                    char *end_quote = strchr(start_quote+1, '\"');
                    name_len = end_quote - start_quote - 1;
                    addr_counter = addr_counter + name_len + 1;
                   }
               }
               else if(strcmp(name, ".END")==0){
                   printf("We have reached the end of the assembly file.\n");
                   break;
               }
               else{
                   printf("We have found an opcode\n");
                   addr_counter = addr_counter + 1;
               }
           }
       }
       line_num++;
    }
    return 0;
}

void print_symbol_table(SymbolTable *st){
    printf("This is our symbol table\n");
    for(int i=0; i<st->count; ++i){
        printf("%s: ", st->my_symbol[i].label_name);
        uint16_t addr = st->my_symbol[i].label_addr;
        int bits = sizeof(addr)*8;
        for(int j=bits-1; j>=0; --j){
            int bit = (addr>>j)&1;
            printf("%d", bit);
            if(j%4==0){
                printf(" ");
            }
        }
        printf("\n");
    }
    return;
}

int main(int argc, char *argv[]){
    if(argc == 1){
        printf("please provide the name of assembly file and .obj file\n");
        return EXIT_FAILURE;
    }
    else if(argc > 3){
        printf("Too many arguments\n");
    }
    else{
        SymbolTable st;
        init_symbol_table(&st);
        char *filepath = argv[1];
        FILE *fp_r = fopen(filepath, "r");
        if(fp_r == NULL){
            printf("unable to open the file: %s in read mode\n", filepath);
            return EXIT_FAILURE;
        }
        // implementing pass 1 and building symbol table
        if(pass1(&st, fp_r) == -1){
            fclose(fp_r);
            return EXIT_FAILURE;
        }
        print_symbol_table(&st);
        // implementing pass 2 and writing raw machine code
        char *file_path = argv[2];
        FILE *fp_w = fopen(file_path, "wb");
        if(fp_w == NULL){
            fprintf(stderr, "some error occurred while opening the file: %s in append mode\n", file_path);
            fclose(fp_r);
            return EXIT_FAILURE;
        }
        fclose(fp_r);
        FILE *again_fp_r = fopen(filepath, "r");
        if(again_fp_r == NULL){
            printf("unable to open the file: %s in read mode\n", filepath);
            return EXIT_FAILURE;
        }
        if(pass2(&st, again_fp_r, fp_w) == -1){
            fclose(again_fp_r);
            fclose(fp_w);
            return EXIT_FAILURE;
        }
        printf("we successfully generated the machine code file\n");
        fclose(fp_w);
        fclose(again_fp_r);
    }
    return EXIT_SUCCESS;
}
