; Diskus P10b — second userland demo (flat binary at 0x40000 when loaded)
BITS 32
ORG 0x40000

start:
    mov eax, 1                  ; SYS_WRITE
    mov ebx, msg
    mov ecx, msg_end - msg
    int 0x80

    mov eax, 3                  ; SYS_EXIT
    int 0x80
    jmp $

msg:
    db "bye from ring3", 0x0A
msg_end:
