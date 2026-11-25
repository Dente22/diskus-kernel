; Diskus P0 — 512-byte BIOS boot sector
; Real mode: print brand on VGA + COM1 (for headless QEMU smoke).
BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    call serial_init

    mov ax, 0x0003          ; 80x25 text
    int 0x10

    mov si, msg
.print:
    lodsb
    test al, al
    jz .hang
    call putc
    jmp .print

.hang:
    hlt
    jmp .hang

; AL = char → VGA teletype + serial
putc:
    push ax
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x0A
    int 0x10
    pop ax
    call serial_putc
    ret

serial_init:
    mov dx, 0x3F8 + 1
    mov al, 0x00            ; disable interrupts
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x80            ; DLAB on
    out dx, al
    mov dx, 0x3F8 + 0
    mov al, 0x03            ; 38400 baud divisor lo (ok for QEMU)
    out dx, al
    mov dx, 0x3F8 + 1
    mov al, 0x00
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x03            ; 8N1
    out dx, al
    mov dx, 0x3F8 + 2
    mov al, 0xC7            ; FIFO
    out dx, al
    mov dx, 0x3F8 + 4
    mov al, 0x0B            ; IRQs, RTS/DSR
    out dx, al
    ret

; AL = char
serial_putc:
    push ax
    push dx
    mov dx, 0x3F8 + 5
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop dx
    pop ax
    push dx
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

msg:
    db "Diskus", 0x0D, 0x0A
    db "kernel P0 - boot OK", 0x0D, 0x0A, 0

    times 510 - ($ - $$) db 0
    dw 0xAA55
