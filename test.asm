; ===================================================
; Complete Test Program for LC-3 Assembler and VM
; ===================================================
.ORIG 0x3000

        ; 1. Print greeting string using LEA and PUTS
        LEA R0, HELLO_MSG
        PUTS

        ; 2. Initialize loop counter R1 = 5
        AND R1, R1, #0      ; R1 = 0
        ADD R1, R1, #5      ; R1 = 5

        ; 3. Loop: Print characters 'A', 'B', 'C', 'D', 'E'
LOOP    LD R0, CHAR_DATA    ; Load current ASCII character into R0
        OUT                 ; Print character to terminal

        ADD R0, R0, #1      ; Increment character ('A' -> 'B' -> etc.)
        ST R0, CHAR_DATA    ; Store updated character back to memory

        ADD R1, R1, #-1     ; Decrement loop counter
        BRP LOOP            ; Loop again while R1 > 0

        ; 4. Print newline
        LD R0, NEWLINE
        OUT

        ; 5. Print success message and HALT
        LEA R0, SUCCESS_MSG
        PUTS

        HALT

; ===================================================
; Data Section
; ===================================================
CHAR_DATA   .FILL 0x0041    ; ASCII for 'A'
NEWLINE     .FILL 0x000A    ; ASCII for '\n'
HELLO_MSG   .STRINGZ "Starting test...\nPrinting: "
SUCCESS_MSG .STRINGZ ">> SUCCESS: All tests passed! <<\n"

.END
