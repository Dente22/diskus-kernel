; Diskus P8 — GDT reload helpers, TSS, enter/exit user mode
BITS 32
GLOBAL gdt_flush
GLOBAL tss_flush
GLOBAL enter_userland
GLOBAL exit_userland
GLOBAL kernel_esp_save

; Fixed RAM for saved kernel ESP (avoid .bss past loaded image)
KERNEL_ESP_SAVE equ 0x00062200

SECTION .text

; void gdt_flush(void *gdt_ptr)
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

; void tss_flush(u16 selector)
tss_flush:
    mov ax, [esp + 4]
    ltr ax
    ret

; void enter_userland(u32 eip, u32 esp)
enter_userland:
    mov eax, [esp + 4]          ; user eip
    mov ebx, [esp + 8]          ; user esp
    mov [KERNEL_ESP_SAVE], esp
    cli
    mov cx, 0x23                ; user data | RPL3
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    push dword 0x23             ; SS
    push ebx                    ; ESP
    pushf
    or dword [esp], 0x200       ; IF=1
    push dword 0x1B             ; CS = user code | RPL3
    push eax                    ; EIP
    iret

; noreturn from SYS_EXIT
exit_userland:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [KERNEL_ESP_SAVE]
    sti
    ret
