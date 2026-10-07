#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
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

int parse_reg(char *str){
    if(!str || *str == '\0'){
        fprintf(stderr, "Pass the register in correct format\n");
        return -1;
    }
    else if(strlen(str) != 2){
        fprintf(stderr, "available general purpose registers are r0/R0, r1/R1 ..... r7/R7. Don't use any other\n");
        return -1;
    }
    int reg_num = str[1] - '0';
    return reg_num;
}

int parse_imm(char *str, int *out_val){
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
    *out_val = (int)val;
    return 0;
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
        FILE *fp = fopen(filepath, "r");
        if(fp == NULL){
            printf("unable to open the file in read mode\n");
            return EXIT_FAILURE;
        }
        if(pass1(&st, fp) == -1){
            return EXIT_FAILURE;
        }
        printf("we have successfully build the symbol table and completed pass 1");
        print_symbol_table(&st);
        fclose(fp);
    }
    return 0;
}
