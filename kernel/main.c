/* Diskus P2 — freestanding C kernel */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static volatile u16 *const VGA = (volatile u16 *)0xB8000;
static u32 cursor;

static void serial_putc(char c) {
  for (;;) {
    u8 status;
    __asm__ volatile("inb %1, %0" : "=a"(status) : "Nd"((u16)0x3FD));
    if (status & 0x20)
      break;
  }
  __asm__ volatile("outb %0, %1" : : "a"(c), "Nd"((u16)0x3F8));
}

static void vga_putc(char c) {
  if (c == '\n') {
    cursor = (cursor / 80 + 1) * 80;
    if (cursor >= 80 * 25)
      cursor = 0;
    serial_putc('\n');
    return;
  }
  VGA[cursor++] = (u16)c | (u16)(0x0A << 8);
  if (cursor >= 80 * 25)
    cursor = 0;
  serial_putc(c);
}

static void puts(const char *s) {
  while (*s)
    vga_putc(*s++);
}

static void vga_clear(void) {
  for (u32 i = 0; i < 80 * 25; i++)
    VGA[i] = (u16)' ' | (u16)(0x0F << 8);
  cursor = 0;
}

void kmain(void) {
  vga_clear();
  puts("Diskus C\n");
  puts("kernel from C - P2 OK\n");
  for (;;)
    __asm__ volatile("hlt");
}
