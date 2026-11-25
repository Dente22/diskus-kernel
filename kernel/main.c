/* Diskus P3 — freestanding C kernel: VGA + PS/2 + serial shell */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

enum { VGA_COLS = 80, VGA_ROWS = 25 };

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 cursor;
static u8 shift_on;

static inline u8 inb(u16 port) {
  u8 v;
  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static inline void outb(u16 port, u8 v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static void serial_putc(char c) {
  while (!(inb(0x3FD) & 0x20))
    ;
  outb(0x3F8, (u8)c);
}

/* non-blocking; -1 if empty */
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

/* Scancode set 1 → ASCII (US), 0 = ignore */
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
    return -1; /* break */
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
  puts("commands: help  clear  about  echo <text>\n");
}

static void cmd_about(void) {
  puts("Diskus kernel P3 - freestanding C, PS/2 + serial shell\n");
}

static void run_line(char *line) {
  /* trim trailing spaces */
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
    if (c < 0)
      continue; /* poll; no IDT yet — do not HLT */
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
  puts("P3 keyboard + shell\n");
  shell();
}
