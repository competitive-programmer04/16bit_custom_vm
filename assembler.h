#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__
#include<stdint.h>
#include<stdio.h>
#define MAX_LABELS 1024
#define MAX_LABEL_LEN 24

typedef struct Symbol{
    char label_name[MAX_LABEL_LEN+1];
    uint16_t label_addr;
}Symbol;

typedef struct SymbolTable{
    Symbol my_symbol[MAX_LABELS];
    int count;
}SymbolTable;

void init_symbol_table(SymbolTable *symbol);
int add_symbol(SymbolTable *symbol, char *name, uint16_t addr);
int check_symbol(SymbolTable *symbol, char *name);
void print_symbol_table(SymbolTable *symbol);
int pass1(SymbolTable *symbol, FILE *file);
uint16_t parse_reg(char *str);
int16_t parse_imm(char *str);
uint16_t encode_instruction(const char *opcode, char *args, uint16_t current_pc, SymbolTable *st, uint16_t line_num);
int pass2(SymbolTable *st, FILE *fp_r, FILE *fp_w);
#endif
