; Kernel entry — called after boot jumps to 0x10000
BITS 32
GLOBAL _start
EXTERN kmain

SECTION .text
_start:
    cli
    mov esp, 0x90000
    call kmain
.hang:
    hlt
    jmp .hang
