; Diskus P1 — BIOS boot → 32-bit protected mode
; Prints "Diskus PM" on VGA (0xB8000) + COM1.
BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    call serial_init

    ; Fast A20
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_desc]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry           ; flush pipeline, CS = code

; ---- 16-bit helpers (before PM) ----
serial_init:
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    ret

; ---- GDT ----
align 8
gdt:
    dq 0                        ; null
    ; code: base0 limit 4G, 32-bit, exec/read
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    ; data: base0 limit 4G, 32-bit, read/write
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

; ---- Protected mode ----
BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; clear first lines of VGA
    mov edi, 0xB8000
    mov ecx, 80 * 2
    mov ax, 0x0F20              ; space, white
    rep stosw

    mov edi, 0xB8000
    mov esi, msg
    mov ah, 0x0A                ; green
.print:
    lodsb
    test al, al
    jz .hang
    cmp al, 0x0A
    je .nl
    mov [edi], ax
    add edi, 2
    call serial_putc32
    jmp .print
.nl:
    ; advance VGA to next row (160 bytes)
    mov eax, edi
    sub eax, 0xB8000
    xor edx, edx
    mov ecx, 160
    div ecx
    inc eax
    imul eax, 160
    add eax, 0xB8000
    mov edi, eax
    mov al, 0x0A
    call serial_putc32
    mov ah, 0x0A
    jmp .print

.hang:
    hlt
    jmp .hang

; AL = char
serial_putc32:
    push eax
    push edx
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop edx
    pop eax
    push edx
    mov dx, 0x3F8
    out dx, al
    pop edx
    ret

msg:
    db "Diskus PM", 0x0A
    db "protected mode OK", 0x0A, 0

    times 510 - ($ - $$) db 0
    dw 0xAA55
