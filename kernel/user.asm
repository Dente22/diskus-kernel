; Diskus P8 — userland demo (flat binary at 0x40000)
BITS 32
ORG 0x40000

start:
    mov eax, 1                  ; SYS_WRITE
    mov ebx, msg
    mov ecx, msg_end - msg
    int 0x80

    mov eax, 2                  ; SYS_UPTIME
    int 0x80                    ; eax = ticks

    mov eax, 3                  ; SYS_EXIT
    int 0x80
    jmp $                       ; never

msg:
    db "hello from ring3", 0x0A
msg_end:
