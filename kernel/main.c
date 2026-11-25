/* Diskus P5 — IRQ keyboard ring + cooperative tasks */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

enum { VGA_COLS = 80, VGA_ROWS = 25, TIMER_HZ = 100, NTASKS = 2 };

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 cursor;
static u8 shift_on;
static volatile u32 ticks;

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void load_idt(void *idtp);
extern void irq_sti(void);
extern void irq_cli(void);
extern void yield(void);

struct idt_entry {
  u16 base_lo;
  u16 sel;
  u8 zero;
  u8 flags;
  u16 base_hi;
} __attribute__((packed));

struct idt_ptr {
  u16 limit;
  u32 base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

struct task {
  u32 esp;
  const char *name;
  u8 alive;
};

/* Task stacks in known RAM (not BSS of the kernel image — objcopy drops BSS). */
#define STACK0_TOP ((u32 *)0x00070000)
#define STACK1_TOP ((u32 *)0x00072000)
static struct task tasks[NTASKS];
static u32 current;
static u32 task_switches;

enum { KBD_CAP = 64 };
static volatile u8 kbd_q[KBD_CAP];
static volatile u8 kbd_head, kbd_tail;

static inline u8 inb(u16 port) {
  u8 v;
  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static inline void outb(u16 port, u8 v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

void timer_handler(void) { ticks++; }

void kbd_irq_handler(void) {
  u8 sc = inb(0x60);
  u8 next = (u8)((kbd_head + 1) % KBD_CAP);
  if (next == kbd_tail)
    return; /* drop */
  kbd_q[kbd_head] = sc;
  kbd_head = next;
}

u32 schedule_switch(u32 old_esp) {
  tasks[current].esp = old_esp;
  u32 next = current;
  for (u32 i = 0; i < NTASKS; i++) {
    next = (next + 1) % NTASKS;
    if (tasks[next].alive) {
      current = next;
      task_switches++;
      return tasks[current].esp;
    }
  }
  return old_esp;
}

static void idt_set(u8 num, u32 base, u16 sel, u8 flags) {
  idt[num].base_lo = (u16)(base & 0xFFFF);
  idt[num].base_hi = (u16)((base >> 16) & 0xFFFF);
  idt[num].sel = sel;
  idt[num].zero = 0;
  idt[num].flags = flags;
}

static void pic_remap(void) {
  outb(0x20, 0x11);
  outb(0xA0, 0x11);
  outb(0x21, 0x20);
  outb(0xA1, 0x28);
  outb(0x21, 0x04);
  outb(0xA1, 0x02);
  outb(0x21, 0x01);
  outb(0xA1, 0x01);
  outb(0x21, 0xFC); /* IRQ0 + IRQ1 */
  outb(0xA1, 0xFF);
}

static void pit_init(u32 hz) {
  u32 div = 1193182 / hz;
  outb(0x43, 0x36);
  outb(0x40, (u8)(div & 0xFF));
  outb(0x40, (u8)((div >> 8) & 0xFF));
}

static void interrupts_init(void) {
  for (u32 i = 0; i < 256; i++)
    idt_set((u8)i, 0, 0x08, 0);
  idt_set(32, (u32)irq0_stub, 0x08, 0x8E);
  idt_set(33, (u32)irq1_stub, 0x08, 0x8E);
  idtp.limit = sizeof(idt) - 1;
  idtp.base = (u32)&idt;
  load_idt(&idtp);
  pic_remap();
  pit_init(TIMER_HZ);
  ticks = 0;
  irq_sti();
}

static void serial_putc(char c) {
  while (!(inb(0x3FD) & 0x20))
    ;
  outb(0x3F8, (u8)c);
}

static int serial_getc(void) {
  if (!(inb(0x3FD) & 0x01))
    return -1;
  return (int)inb(0x3F8);
}

static void vga_clear(void) {
  for (u32 i = 0; i < VGA_COLS * VGA_ROWS; i++)
    VGA[i] = (u16)' ' | (u16)(0x0F << 8);
  cursor = 0;
}

static void vga_putc(char c) {
  if (c == '\n') {
    cursor = (cursor / VGA_COLS + 1) * VGA_COLS;
    if (cursor >= VGA_COLS * VGA_ROWS)
      cursor = 0;
    serial_putc('\n');
    return;
  }
  if (c == '\b') {
    if (cursor % VGA_COLS) {
      cursor--;
      VGA[cursor] = (u16)' ' | (u16)(0x0A << 8);
    }
    serial_putc('\b');
    serial_putc(' ');
    serial_putc('\b');
    return;
  }
  VGA[cursor++] = (u16)c | (u16)(0x0A << 8);
  if (cursor >= VGA_COLS * VGA_ROWS)
    cursor = 0;
  serial_putc(c);
}

static void puts(const char *s) {
  while (*s)
    vga_putc(*s++);
}

static void put_u32(u32 n) {
  char buf[11];
  int i = 0;
  if (n == 0) {
    vga_putc('0');
    return;
  }
  while (n && i < 10) {
    buf[i++] = (char)('0' + (n % 10));
    n /= 10;
  }
  while (i--)
    vga_putc(buf[i]);
}

static const char MAP[58] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,    ' ',
};

static const char MAP_SHIFT[58] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,    ' ',
};

static int scancode_to_ascii(u8 sc) {
  if (sc == 0x2A || sc == 0x36) {
    shift_on = 1;
    return -1;
  }
  if (sc == 0xAA || sc == 0xB6) {
    shift_on = 0;
    return -1;
  }
  if (sc & 0x80)
    return -1;
  if (sc >= sizeof(MAP))
    return -1;
  char ch = shift_on ? MAP_SHIFT[sc] : MAP[sc];
  return ch ? (int)ch : -1;
}

static int kbd_getc(void) {
  if (kbd_tail == kbd_head)
    return -1;
  u8 sc = kbd_q[kbd_tail];
  kbd_tail = (u8)((kbd_tail + 1) % KBD_CAP);
  return scancode_to_ascii(sc);
}

static int getc(void) {
  int c = kbd_getc();
  if (c >= 0)
    return c;
  return serial_getc();
}

static int streq(const char *a, const char *b) {
  while (*a && *b && *a == *b) {
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static int starts_with(const char *s, const char *p) {
  while (*p) {
    if (*s++ != *p++)
      return 0;
  }
  return 1;
}

static void cmd_help(void) {
  puts("commands: help clear about echo uptime tasks\n");
}

static void cmd_about(void) {
  puts("Diskus kernel P5 - IRQ1 kbd ring + cooperative tasks\n");
}

static void cmd_uptime(void) {
  u32 t = ticks;
  u32 sec = t / TIMER_HZ;
  u32 cs = t % TIMER_HZ;
  puts("uptime ");
  put_u32(sec);
  vga_putc('.');
  if (cs < 10)
    vga_putc('0');
  put_u32(cs);
  puts(" s (");
  put_u32(t);
  puts(" ticks)\n");
}

static void cmd_tasks(void) {
  puts("tasks:\n");
  for (u32 i = 0; i < NTASKS; i++) {
    if (!tasks[i].alive)
      continue;
    puts("  ");
    put_u32(i);
    puts(" ");
    puts(tasks[i].name);
    if (i == current)
      puts(" *");
    puts("\n");
  }
  puts("switches ");
  put_u32(task_switches);
  puts("\n");
}

static void run_line(char *line) {
  char *end = line;
  while (*end)
    end++;
  while (end > line && (end[-1] == ' ' || end[-1] == '\t'))
    *--end = 0;

  if (!line[0])
    return;
  if (streq(line, "help") || streq(line, "?")) {
    cmd_help();
    return;
  }
  if (streq(line, "clear")) {
    vga_clear();
    return;
  }
  if (streq(line, "about")) {
    cmd_about();
    return;
  }
  if (streq(line, "uptime")) {
    cmd_uptime();
    return;
  }
  if (streq(line, "tasks")) {
    cmd_tasks();
    return;
  }
  if (starts_with(line, "echo ")) {
    puts(line + 5);
    puts("\n");
    return;
  }
  if (streq(line, "echo")) {
    puts("\n");
    return;
  }
  puts("unknown: ");
  puts(line);
  puts(" (try help)\n");
}

static void shell(void) {
  char buf[96];
  u32 len = 0;

  puts("Diskus shell - type help\n");
  puts("> ");

  for (;;) {
    int c = getc();
    if (c < 0) {
      yield();
      continue;
    }
    if (c == '\r')
      c = '\n';

    if (c == '\n') {
      vga_putc('\n');
      buf[len] = 0;
      run_line(buf);
      len = 0;
      puts("> ");
      continue;
    }
    if (c == '\b') {
      if (len) {
        len--;
        vga_putc('\b');
      }
      continue;
    }
    if (c < 32 || c > 126)
      continue;
    if (len + 1 >= sizeof(buf))
      continue;
    buf[len++] = (char)c;
    vga_putc((char)c);
  }
}

static void task_blink(void) {
  static const char spin[] = "|/-\\";
  u32 i = 0;
  for (;;) {
    VGA[VGA_COLS - 1] = (u16)spin[i % 4] | (u16)(0x0E << 8);
    i++;
    yield();
  }
}

static void task_exit(void) {
  tasks[current].alive = 0;
  for (;;)
    yield();
}

static void task_spawn(u32 id, const char *name, void (*entry)(void), u32 *stack_top) {
  u32 *sp = stack_top;
  /* After yield: popa then ret. popa needs EDI..EAX then return EIP. */
  *--sp = (u32)task_exit; /* if entry returns */
  *--sp = (u32)entry;     /* EIP */
  *--sp = 0;              /* EAX */
  *--sp = 0;              /* ECX */
  *--sp = 0;              /* EDX */
  *--sp = 0;              /* EBX */
  *--sp = 0;              /* ESP (discarded) */
  *--sp = 0;              /* EBP */
  *--sp = 0;              /* ESI */
  *--sp = 0;              /* EDI <- ESP */
  tasks[id].esp = (u32)sp;
  tasks[id].name = name;
  tasks[id].alive = 1;
}

static void tasks_init(void) {
  current = 0;
  task_switches = 0;
  tasks[0].name = "shell";
  tasks[0].alive = 1;
  tasks[0].esp = 0; /* set on first yield */
  (void)STACK0_TOP;  /* shell uses current ESP from kmain */
  task_spawn(1, "blink", task_blink, STACK1_TOP);
}

void kmain(void) {
  vga_clear();
  puts("Diskus C\n");
  puts("P5 IRQ1 + cooperative tasks\n");
  interrupts_init();
  puts("irq ok\n");
  tasks_init();
  puts("tasks ok\n");

  u32 start = ticks;
  u32 spins = 0;
  while (ticks - start < 20) {
    spins++;
    if (spins > 200000000u) {
      puts("timer fail\n");
      break;
    }
  }
  puts("ticks ");
  put_u32(ticks);
  puts("\n");

  shell();
}
