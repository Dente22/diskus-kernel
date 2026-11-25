; Diskus P5 — IRQ0/IRQ1 stubs, lidt, yield
BITS 32
GLOBAL irq0_stub
GLOBAL irq1_stub
GLOBAL load_idt
GLOBAL irq_sti
GLOBAL irq_cli
GLOBAL yield
EXTERN timer_handler
EXTERN kbd_irq_handler
EXTERN schedule_switch

SECTION .text

irq0_stub:
    pusha
    call timer_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

irq1_stub:
    pusha
    call kbd_irq_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

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

; Cooperative yield: save regs, ask C for next ESP, restore
yield:
    pusha
    push esp
    call schedule_switch      ; u32 schedule_switch(u32 old_esp)
    add esp, 4
    mov esp, eax
    popa
    ret
