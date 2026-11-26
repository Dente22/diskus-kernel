; Diskus P12 — cat readme via SYS_OPEN/READ/WRITE/CLOSE
BITS 32
ORG 0x40000

SYS_WRITE equ 1
SYS_EXIT  equ 3
SYS_OPEN  equ 4
SYS_READ  equ 5
SYS_CLOSE equ 6

start:
    mov eax, SYS_OPEN
    mov ebx, fname
    int 0x80
    cmp eax, 0xFFFFFFFF
    je fail
    mov esi, eax                ; fd

    sub esp, 128                ; buf on stack (keep binary small)
    mov eax, SYS_READ
    mov ebx, esi
    mov ecx, esp
    mov edx, 128
    int 0x80
    mov edi, eax                ; nbytes
    cmp edi, 0
    jle close

    mov eax, SYS_WRITE
    mov ebx, esp
    mov ecx, edi
    int 0x80

close:
    add esp, 128
    mov eax, SYS_CLOSE
    mov ebx, esi
    int 0x80
    mov eax, SYS_EXIT
    int 0x80
    jmp $

fail:
    mov eax, SYS_WRITE
    mov ebx, errmsg
    mov ecx, errmsg_end - errmsg
    int 0x80
    mov eax, SYS_EXIT
    int 0x80
    jmp $

fname:
    db "readme", 0
errmsg:
    db "open fail", 0x0A
errmsg_end:
