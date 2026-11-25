; Diskus — IRQ0/IRQ1, int 0x80 syscall, lidt, yield
BITS 32
GLOBAL irq0_stub
GLOBAL irq1_stub
GLOBAL syscall_stub
GLOBAL load_idt
GLOBAL irq_sti
GLOBAL irq_cli
GLOBAL yield
EXTERN timer_handler
EXTERN kbd_irq_handler
EXTERN schedule_switch
EXTERN syscall_dispatch

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

; int 0x80: eax=num, ebx,ecx,edx=args → eax=result
syscall_stub:
    push edx
    push ecx
    push ebx
    push eax
    call syscall_dispatch
    add esp, 16
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

yield:
    pusha
    push esp
    call schedule_switch
    add esp, 4
    mov esp, eax
    popa
    ret
