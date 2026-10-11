CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L

TARGET_VM = vm
TARGET_ASM = assembler

SRCS_VM = vm.c
SRCS_ASM = assembler.c

all: $(TARGET_VM) $(TARGET_ASM)

$(TARGET_VM): $(SRCS_VM) vm.h
	$(CC) $(CFLAGS) $(SRCS_VM) -o $(TARGET_VM)

$(TARGET_ASM): $(SRCS_ASM) assembler.h
	$(CC) $(CFLAGS) $(SRCS_ASM) -o $(TARGET_ASM)
	
clean:
	rm -f $(TARGET_VM) $(TARGET_ASM) *.obj

.PHONY: all clean
