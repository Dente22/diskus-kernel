; Diskus P2 — load C kernel from disk, enter PM, jump to 0x10000
BITS 16
ORG 0x7C00

KERNEL_SEG  equ 0x1000          ; physical 0x10000
KERNEL_SECS equ 64              ; 32 KiB max for now

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl        ; BIOS drive number

    call serial_init

    ; Load kernel: CHS start sector 2 (after MBR/boot)
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx                  ; ES:BX = 0x1000:0000
    mov ah, 0x02                ; read sectors
    mov al, KERNEL_SECS
    mov ch, 0                   ; cylinder
    mov cl, 2                   ; sector (1-based; 1=boot)
    mov dh, 0                   ; head
    mov dl, [boot_drive]
    int 0x13
    jc disk_fail

    ; Fast A20
    in al, 0x92
    or al, 2
    out 0x92, al

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_entry

disk_fail:
    mov si, err_disk
.f:
    lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    int 0x10
    call serial_putc16
    jmp .f
.h:
    hlt
    jmp .h

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

serial_putc16:
    push ax
    push dx
    mov dx, 0x3FD
.w:
    in al, dx
    test al, 0x20
    jz .w
    pop dx
    pop ax
    push dx
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

align 8
gdt:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

boot_drive: db 0
err_disk: db "disk read fail", 0x0A, 0

BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000                 ; kernel entry

    times 510 - ($ - $$) db 0
    dw 0xAA55
