; Diskus — IRQ0/IRQ1/IRQ12, int 0x80 syscall, lidt, yield (iret-frame)
BITS 32
GLOBAL irq0_stub
GLOBAL irq1_stub
GLOBAL irq12_stub
GLOBAL syscall_stub
GLOBAL load_idt
GLOBAL irq_sti
GLOBAL irq_cli
GLOBAL yield
GLOBAL exit_current_task
EXTERN timer_handler
EXTERN kbd_irq_handler
EXTERN mouse_irq_handler
EXTERN schedule_switch
EXTERN schedule_after_exit
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
    ; Schedule from ring0 and ring3 (ring3 frame lives on TSS.esp0 stack)
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

; IRQ12 mouse (slave) — EOI slave then master
irq12_stub:
    pusha
    mov ax, 0x10
    mov ds, ax
    call mouse_irq_handler
    mov al, 0x20
    out 0xA0, al
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

; noreturn — SYS_EXIT already marked task dead
exit_current_task:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    call schedule_after_exit
    mov esp, eax
    popa
    iret
