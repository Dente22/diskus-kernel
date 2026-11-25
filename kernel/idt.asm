; Diskus — IRQ0/IRQ1, int 0x80 syscall, lidt, yield (iret-frame)
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
EXTERN preempt_enabled

SECTION .text

; Stack after pusha: [EDI..EAX][EIP][CS][EFLAGS]
irq0_stub:
    pusha
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    call timer_handler
    mov al, 0x20
    out 0x20, al                ; EOI

    cmp dword [preempt_enabled], 0
    je .nosched
    ; CS of interrupted code at [esp+36]
    mov eax, [esp + 36]
    test eax, 3                 ; RPL != 0 → ring3, don't steal user context
    jnz .nosched

    push esp
    call schedule_switch
    add esp, 4
    mov esp, eax
.nosched:
    popa
    iret

irq1_stub:
    pusha
    mov ax, 0x10
    mov ds, ax
    call kbd_irq_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

; int 0x80: eax=num, ebx,ecx,edx=args → eax=result
syscall_stub:
    push ds
    push edx
    push ecx
    push ebx
    push eax
    mov ax, 0x10
    mov ds, ax
    call syscall_dispatch
    add esp, 16
    pop ds
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

; Cooperative yield — same stack shape as IRQ (pusha + iret frame)
yield:
    pop eax                     ; return EIP
    pushf
    or dword [esp], 0x200       ; IF=1 on resume
    push dword 0x08             ; kernel CS
    push eax                    ; EIP
    pusha
    push esp
    call schedule_switch
    add esp, 4
    mov esp, eax
    popa
    iret
