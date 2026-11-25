/* Diskus P4 — VGA + PS/2 + serial shell + PIT timer (IRQ0) */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

enum { VGA_COLS = 80, VGA_ROWS = 25, TIMER_HZ = 100 };

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 cursor;
static u8 shift_on;
static volatile u32 ticks;

extern void irq0_stub(void);
extern void load_idt(void *idtp);
extern void irq_sti(void);
extern void irq_cli(void);

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

static inline u8 inb(u16 port) {
  u8 v;
  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static inline void outb(u16 port, u8 v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

void timer_handler(void) { ticks++; }

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
  outb(0x21, 0x20); /* master IRQs -> INT 32 */
  outb(0xA1, 0x28); /* slave  -> INT 40 */
  outb(0x21, 0x04);
  outb(0xA1, 0x02);
  outb(0x21, 0x01);
  outb(0xA1, 0x01);
  outb(0x21, 0xFE); /* unmask IRQ0 only */
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
  idt_set(32, (u32)irq0_stub, 0x08, 0x8E); /* present, DPL0, 32-bit interrupt gate */
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

static int kbd_getc(void) {
  if (!(inb(0x64) & 1))
    return -1;
  u8 sc = inb(0x60);
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
  puts("commands: help  clear  about  echo <text>  uptime\n");
}

static void cmd_about(void) {
  puts("Diskus kernel P4 - freestanding C, PS/2 shell, PIT timer\n");
}

static void cmd_uptime(void) {
  u32 t = ticks;
  u32 sec = t / TIMER_HZ;
  u32 cs = t % TIMER_HZ; /* centiseconds-ish at 100Hz */
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
      __asm__ volatile("hlt"); /* wake on IRQ0 */
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

void kmain(void) {
  vga_clear();
  puts("Diskus C\n");
  puts("P4 IDT + PIT timer\n");
  interrupts_init();

  /* ~0.3s for smoke: prove ticks advance */
  u32 start = ticks;
  while (ticks - start < 30)
    __asm__ volatile("hlt");
  puts("ticks ");
  put_u32(ticks);
  puts("\n");

  shell();
}
