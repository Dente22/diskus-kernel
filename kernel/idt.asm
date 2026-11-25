; Diskus P4 — IRQ0 stub + lidt
BITS 32
GLOBAL irq0_stub
GLOBAL load_idt
GLOBAL irq_sti
GLOBAL irq_cli
EXTERN timer_handler

SECTION .text

irq0_stub:
    pusha
    call timer_handler
    mov al, 0x20
    out 0x20, al              ; PIC master EOI
    popa
    iret

; void load_idt(void *idtp)
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

irq_sti:
    sti
    ret

irq_cli:
    cli
    ret
