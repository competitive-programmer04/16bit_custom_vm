#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdint.h>
#include<sys/select.h>
#include<sys/time.h>
#include<termios.h>
#include<errno.h>
#include<stdlib.h>
#include"vm.h"

int running = 1;
struct termios original_struct;

void disable_input_buffering(){
    tcgetattr(STDIN_FILENO, &original_struct);
    struct termios raw_struct = original_struct;
    raw_struct.c_lflag = raw_struct.c_lflag & (~(ICANON|ECHO));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_struct);
    return;
}

void enable_input_buffering(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_struct);
    return;
}

int check_key_press(){
    fd_set read_fds; // a set of file descriptors
    FD_ZERO(&read_fds); // making every bit in read_fds to 0
    FD_SET(STDIN_FILENO, &read_fds); // read_fds will now represent STDIN_FILENO
    struct timeval timeout;
    timeout.tv_sec = 0; // seconds
    timeout.tv_usec = 0; // microseonds
    return select(STDIN_FILENO+1, &read_fds, NULL, NULL, &timeout) > 0;
    /*
     int selct(int nfds, fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, timeval *timeout)
     nfds -> maximum file descriptor jo monitor karna hai + 1
     read_fds -> set of readable file de4scriptors ko point karne ke liye
     write_fds -> set of writable file descriptors ko point karne ke liye
     except_fds -> set of file descriptors waiting for exceptional case (like out of band network error)
     timeout -> NULL -> it will wait forever until user presses a key
                {0,0} -> instant polling
                {seconds, microseonds} -> wait for that much amount of time and it can also return before
                return 0 -> timeout
                       1 -> file descriptors are ready for reading or writing
                       -1 -> error
     */
    return;
}

uint16_t mem_read(uint16_t addr){
    if(addr == ADDR_KBSR){
        if(check_key_press()){
            memory[ADDR_KBSR] = (1<<15); // bit 15 = 1 indicating that user is pressed a key
            uint8_t ch;
            read(STDIN_FILENO, &ch, 1);
            memory[ADDR_KBDR] = ch;
        }
        else{
            memory[ADDR_KBSR] = 0;
        }
        return memory[ADDR_KBSR];
    }
    else if(addr == ADDR_KBDR){
        return memory[ADDR_KBDR];
    }
    else{
        return memory[addr];
    }
}

void mem_write(uint16_t addr, uint16_t val){
    memory[addr] = val;
    return;
}

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
            registers[dr] = mem_read(registers[pc] + pc_offset_9);
            set_condition_code(registers[dr]);
            break;
        }
        case OP_LDI:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            registers[dr] = mem_read(mem_read(registers[pc]+ pc_offset_9));
            set_condition_code(registers[dr]);
            break;
        }
        case OP_LDR:{
            uint16_t dr = (ins>>9)&0x7;
            uint16_t base_r = (ins>>6)&0x7;
            uint16_t pc_offset_6 = sign_extend(ins&0x3F,6);
            registers[dr] = mem_read(registers[base_r] + pc_offset_6);
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
            mem_write(registers[pc] + pc_offset_9, registers[sr]);
            break;
        }
        case OP_STI:{
            uint16_t sr = (ins>>9)&0x7;
            uint16_t pc_offset_9 = sign_extend(ins&0x1FF, 9);
            mem_write(mem_read(registers[pc]+pc_offset_9), registers[sr]);
            break;
        }
        case OP_STR:{
            uint16_t sr = (ins>>9)&0x7;
            uint16_t base_r = (ins>>6)&0x7;
            uint16_t pc_offset_6 = sign_extend(ins&0x3F, 6);
            mem_write(registers[base_r]+pc_offset_6, registers[sr]);
            break;
        }
        case OP_RTI:{
            if((registers[psr]>>15)&1){
                char *mesg = "Privilege mode violation. Cannot execute RTI in user mode\n";
                size_t len = 0;
                while(mesg[len] != '\n'){
                    len++;
                }
                write(STDOUT_FILENO, mesg, len+1);
                running = 0;
            }
            break;
        }
        case OP_TRAP:{
            registers[r7] = registers[pc];
            uint16_t trapvect8 = ins&0xFF;
            switch(trapvect8){
                case TRAP_GETC:{
                    uint8_t ch;
                    read(STDIN_FILENO, &ch, 1);
                    registers[r0] = ch;
                    set_condition_code(registers[r0]);
                    break;
                }
                case TRAP_OUT:{
                    char ch = registers[r0]&0xFF;
                    write(STDOUT_FILENO, &ch, 1);
                    break;
                }
                case TRAP_PUTS:{
                   uint16_t addr = registers[r0];
                   uint16_t val = mem_read(addr);
                   while(val != 0x0000){
                       char ch= val&0xFF;
                       write(STDOUT_FILENO, &ch, 1);
                       val = memory[++addr];
                   }
                   break;
                }
                case TRAP_IN:{
                   char *str = "Print a character on the screen\n";
                   size_t len =0;
                   while(str[len] != '\n'){
                       len++;
                   }
                   write(STDOUT_FILENO, str, len+1);
                   uint8_t ch;
                   read(STDIN_FILENO, &ch, 1);
                   registers[r0] = ch;
                   set_condition_code(registers[r0]);
                   write(STDOUT_FILENO, &ch, 1);
                   break;
                }
                case TRAP_PUTSP:{
                   uint16_t addr = registers[r0];
                   uint16_t val_l = mem_read(addr)&0xFF;
                   uint16_t val_h = (mem_read(addr)>>8)&0xFF;
                   while(mem_read(addr) != 0x0000){
                       char ch1 = val_l;
                       write(STDOUT_FILENO, &ch1, 1);
                       if(val_h == 0x00) break;
                       char ch2 = val_h;
                       write(STDOUT_FILENO, &ch2, 1);
                       val_l = mem_read(++addr)&0xFF;
                       val_h = (mem_read(addr)>>8)&0xFF;
                   }
                   break;
                }
                case TRAP_HLT:{
                   running = 0;
                   break;
                }
                default:{
                   char *mesg = "Invalid trap vector 8 bit\n";
                   size_t len = 0;
                   while(mesg[len] != '\n'){
                       len++;
                   }
                   write(STDOUT_FILENO, mesg, len+1);
                   break;
              }
            }
        }
    }
    return;
}

int main(int argc, char *argv[]){
    if(argc < 2){
        char *msg = "Also pass the path of .obj file\n";
        size_t len = 0;
        while(msg[len] != '\n'){
            len++;
        }
        write(STDOUT_FILENO, msg, len+1);
        return 1;
    }
    else if(argc > 2){
        char *msg = "Too many arguments\n";
        size_t len = 0;
        while(msg[len] != '\n'){
            len++;
        }
        write(STDOUT_FILENO, msg, len+1);
        return 1;
    }
    for(size_t i=0; i<NUM_REG; ++i){
        registers[i] = 0;
    }
    for(size_t i=0; i<NUM_ADDR; ++i){
        memory[i] = 0;
    }
    char *fpath = argv[1];
    int fd = open(fpath, O_RDONLY);
    if (fd == -1){
        perror("open");
        return 1;
    }
    uint16_t origin;
    read(fd, &origin, sizeof(origin));
    origin = ((origin << 8)|(origin >> 8));
    if(origin >= MAX_USR_SPACE){
        fprintf(stderr, "user space limit crossed\n");
        return 1;
    }
    size_t max_space_left = (MAX_USR_SPACE - origin)*2;
    size_t read_bytes = 0;
    size_t idx = 0;
    while(read_bytes <= max_space_left){
        size_t num_read = read(fd, &memory[origin+idx] , sizeof(memory[origin+idx]));
        if(num_read == 0){
            break; // we reached EOF
        }
        memory[origin+idx] = ((memory[origin+idx] << 8)|(memory[origin+idx] >> 8)); // converting big-endian to little endian
        read_bytes = read_bytes + num_read;
        idx++;
    }
    
    disable_input_buffering();
    atexit(enable_input_buffering);

    registers[pc] = origin;

    while(running){
        fetch_decode_execute();
    }
    return 0;
}