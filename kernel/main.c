/* Diskus P18 — multi-window desktop + mouse */
#include "desktop.h"
#include "disk.h"
#include "gfx.h"
#include "mouse.h"
#include "paging.h"
#include "ramfs.h"
#include "ui.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

enum {
  TIMER_HZ = 100,
  NTASKS = 3,
  TASK_USER = 2,
  SYS_WRITE = 1,
  SYS_UPTIME = 2,
  SYS_EXIT = 3,
  SYS_OPEN = 4,
  SYS_READ = 5,
  SYS_CLOSE = 6,
  USER_ENTRY = 0x40000,
  USER_STACK = 0x50000,
  FD_MAX = 4,
};

static u8 shift_on;
static volatile u32 ticks;
/* Set after tasks_init; IRQ0 skips schedule when 0. Asm reads this symbol. */
volatile u32 preempt_enabled;

extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq12_stub(void);
extern void syscall_stub(void);
extern void load_idt(void *idtp);
extern void irq_sti(void);
extern void irq_cli(void);
extern void yield(void);
extern void gdt_flush(void *gdt_ptr);
extern void tss_flush(u16 selector);
extern void enter_userland(u32 eip, u32 esp);
extern void exit_userland(void);
extern void exit_current_task(void);

static void puts(const char *s);
static void put_u32(u32 n);
static void vga_putc(char c);

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

struct gdt_ptr {
  u16 limit;
  u32 base;
} __attribute__((packed));

struct tss_entry {
  u32 prev;
  u32 esp0;
  u32 ss0;
  u32 unused[19];
  u16 trap;
  u16 iomap;
} __attribute__((packed));

/* Fixed RAM — avoid BSS past the loaded kernel image */
#define IDT_MEM ((struct idt_entry *)0x00061000)
#define GDT_MEM ((u64 *)0x00062000)
#define TSS_MEM ((struct tss_entry *)0x00062100)

struct fdent {
  u8 used;
  u32 pos;
  u32 len;
  u8 data[RAMFS_DATA];
};
#define FD_MEM ((struct fdent *)0x00067000)

static struct idt_ptr idtp;
static struct gdt_ptr gdtp;

struct task {
  u32 esp;
  u32 kstack_top; /* TSS.esp0 when this task is in ring3 */
  const char *name;
  u8 alive;
  u8 user; /* 1 = ring3 process */
};

/* Task stacks in known RAM (not BSS of the kernel image — objcopy drops BSS). */
#define STACK0_TOP ((u32 *)0x00070000)
#define STACK1_TOP ((u32 *)0x00072000)
#define STACK2_TOP ((u32 *)0x00074000) /* kernel stack for user task / TSS.esp0 */
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

void kbd_push_scancode(u8 sc) {
  u8 next = (u8)((kbd_head + 1) % KBD_CAP);
  if (next == kbd_tail)
    return;
  kbd_q[kbd_head] = sc;
  kbd_head = next;
}

void kbd_irq_handler(void) {
  /* Always drain OBF; AUX bit chooses mouse vs keyboard. */
  for (u32 n = 0; n < 16; n++) {
    u8 st = inb(0x64);
    if (!(st & 0x01))
      break;
    u8 b = inb(0x60);
    if (st & 0x20)
      mouse_on_byte(b);
    else
      kbd_push_scancode(b);
  }
}

/* Poll 8042 in case IRQs are missed (common while debugging). */
static void kbd_poll(void) {
  for (u32 n = 0; n < 16; n++) {
    u8 st = inb(0x64);
    if (!(st & 0x01))
      break;
    u8 b = inb(0x60);
    if (st & 0x20)
      mouse_on_byte(b);
    else
      kbd_push_scancode(b);
  }
}

static void tss_set_esp0(u32 esp0) { TSS_MEM->esp0 = esp0; }

static void activate_task(u32 id) {
  current = id;
  if (tasks[id].user)
    tss_set_esp0(tasks[id].kstack_top);
  else
    tss_set_esp0((u32)STACK2_TOP);
}

u32 schedule_switch(u32 old_esp) {
  tasks[current].esp = old_esp;
  u32 next = current;
  for (u32 i = 0; i < NTASKS; i++) {
    next = (next + 1) % NTASKS;
    if (tasks[next].alive) {
      activate_task(next);
      task_switches++;
      return tasks[current].esp;
    }
  }
  return old_esp;
}

/* SYS_EXIT: current already !alive — pick another task (noreturn via asm). */
u32 schedule_after_exit(void) {
  u32 next = current;
  for (u32 i = 0; i < NTASKS; i++) {
    next = (next + 1) % NTASKS;
    if (tasks[next].alive) {
      activate_task(next);
      task_switches++;
      return tasks[current].esp;
    }
  }
  for (;;)
    __asm__ volatile("hlt");
}

static void idt_set(u8 num, u32 base, u16 sel, u8 flags) {
  struct idt_entry *idt = IDT_MEM;
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
  outb(0x21, 0xFC); /* IRQ0 + IRQ1 (+ IRQ2 cascade already clear) */
  outb(0xA1, 0xEF); /* IRQ12 mouse */
}

static void pit_init(u32 hz) {
  u32 div = 1193182 / hz;
  outb(0x43, 0x36);
  outb(0x40, (u8)(div & 0xFF));
  outb(0x40, (u8)((div >> 8) & 0xFF));
}

static void gdt_set(int num, u32 base, u32 limit, u8 access, u8 gran) {
  u64 *gdt = GDT_MEM;
  gdt[num] = 0;
  gdt[num] |= (u64)(limit & 0xFFFF);
  gdt[num] |= (u64)(base & 0xFFFF) << 16;
  gdt[num] |= (u64)((base >> 16) & 0xFF) << 32;
  gdt[num] |= (u64)access << 40;
  gdt[num] |= (u64)((limit >> 16) & 0x0F) << 48;
  gdt[num] |= (u64)(gran & 0xF0) << 48;
  gdt[num] |= (u64)((base >> 24) & 0xFF) << 56;
}

static void gdt_tss_init(void) {
  struct tss_entry *tss = TSS_MEM;
  for (u32 i = 0; i < sizeof(*tss) / 4; i++)
    ((u32 *)tss)[i] = 0;
  /* Kernel stack for ring3→ring0 (per-user kstack updated on schedule) */
  tss->esp0 = (u32)STACK2_TOP;
  tss->ss0 = 0x10;
  tss->iomap = (u16)sizeof(*tss);

  gdt_set(0, 0, 0, 0, 0);
  gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); /* kernel code */
  gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); /* kernel data */
  gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF); /* user code DPL3 */
  gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF); /* user data DPL3 */
  gdt_set(5, (u32)tss, sizeof(*tss) - 1, 0x89, 0x00);

  gdtp.limit = (u16)(6 * 8 - 1);
  gdtp.base = (u32)GDT_MEM;
  gdt_flush(&gdtp);
  tss_flush(0x28);
}

/* interrupts_init inlined in kmain for P8 bring-up markers */

static int fd_alloc(void) {
  struct fdent *fds = FD_MEM;
  for (u32 i = 0; i < FD_MAX; i++) {
    if (!fds[i].used)
      return (int)i;
  }
  return -1;
}

static void fds_init(void) {
  struct fdent *fds = FD_MEM;
  for (u32 i = 0; i < FD_MAX; i++)
    fds[i].used = 0;
}

/* Copy C string from user pointer (identity map). */
static int copy_uname(const char *upath, char *out, u32 cap) {
  if (!upath || cap == 0)
    return -1;
  for (u32 i = 0; i + 1 < cap; i++) {
    char c = upath[i];
    out[i] = c;
    if (!c)
      return 0;
  }
  out[cap - 1] = 0;
  return 0;
}

/* Called from asm int 0x80: num=eax, arg1=ebx, arg2=ecx, arg3=edx */
u32 syscall_dispatch(u32 num, u32 arg1, u32 arg2, u32 arg3) {
  if (num == SYS_WRITE) {
    const char *p = (const char *)arg1;
    u32 len = arg2;
    for (u32 i = 0; i < len && p; i++)
      vga_putc(p[i]);
    return len;
  }
  if (num == SYS_UPTIME)
    return ticks;
  if (num == SYS_EXIT) {
    tasks[current].alive = 0;
    exit_current_task(); /* noreturn — schedule another task */
  }
  if (num == SYS_OPEN) {
    char name[RAMFS_NAME];
    if (copy_uname((const char *)arg1, name, sizeof(name)) < 0)
      return (u32)-1;
    int fd = fd_alloc();
    if (fd < 0)
      return (u32)-1;
    struct fdent *f = &FD_MEM[fd];
    int n = ramfs_get(name, f->data, RAMFS_DATA);
    if (n < 0)
      return (u32)-1;
    f->used = 1;
    f->pos = 0;
    f->len = (u32)n;
    return (u32)fd;
  }
  if (num == SYS_READ) {
    u32 fd = arg1;
    u8 *dst = (u8 *)arg2;
    u32 want = arg3;
    if (fd >= FD_MAX || !FD_MEM[fd].used || !dst)
      return (u32)-1;
    struct fdent *f = &FD_MEM[fd];
    u32 left = f->len - f->pos;
    if (want > left)
      want = left;
    for (u32 i = 0; i < want; i++)
      dst[i] = f->data[f->pos + i];
    f->pos += want;
    return want;
  }
  if (num == SYS_CLOSE) {
    u32 fd = arg1;
    if (fd >= FD_MAX || !FD_MEM[fd].used)
      return (u32)-1;
    FD_MEM[fd].used = 0;
    return 0;
  }
  return (u32)-1;
}

static void task_spawn_user(u32 id, const char *name, u32 entry, u32 uesp, u32 *kstack_top);

/* Load from ramfs, spawn as schedulable ring3 task, wait until exit. */
static int run_from_ramfs(const char *name) {
  if (tasks[TASK_USER].alive) {
    puts("user busy\n");
    return -1;
  }
  int n = ramfs_get(name, (void *)USER_ENTRY, RAMFS_DATA);
  if (n < 0) {
    puts("no such program: ");
    puts(name);
    puts("\n");
    return -1;
  }
  if (n == 0) {
    puts("empty program\n");
    return -1;
  }
  puts("spawned ");
  puts(name);
  puts(" ");
  put_u32((u32)n);
  puts(" bytes\n");
  task_spawn_user(TASK_USER, name, USER_ENTRY, USER_STACK, STACK2_TOP);
  while (tasks[TASK_USER].alive)
    __asm__ volatile("hlt");
  puts("user done\n");
  return 0;
}

static u32 dosyscall(u32 num, u32 a, u32 b, u32 c) {
  u32 ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
  return ret;
}

static u32 strlen_s(const char *s) {
  u32 n = 0;
  while (s[n])
    n++;
  return n;
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
  gfx_clear_text();
}

static void vga_putc(char c) {
  serial_putc(c);
  gfx_putc(c, 10); /* light green on black */
  if (c == '\b') {
    serial_putc(' ');
    serial_putc('\b');
  }
}

static void puts(const char *s) {
  while (*s)
    vga_putc(*s++);
}

static void sys_puts(const char *s) {
  dosyscall(SYS_WRITE, (u32)s, strlen_s(s), 0);
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

static int kbd_pop_scancode(void) {
  if (kbd_tail == kbd_head)
    return -1;
  u8 sc = kbd_q[kbd_tail];
  kbd_tail = (u8)((kbd_tail + 1) % KBD_CAP);
  return (int)sc;
}

static int getc(void) {
  /* Prefer serial (stdio) — reliable under WSL QEMU */
  int c = serial_getc();
  if (c >= 0)
    return c;
  int sc = kbd_pop_scancode();
  if (sc < 0)
    return -1;
  /* Arrows / space-click when line empty handled in shell */
  return scancode_to_ascii((u8)sc);
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
  puts("commands: help clear about echo uptime tasks ls cat write sysdemo run ui m win\n");
  puts("  ui [1|2]  — Yes/No in App window\n");
  puts("  m <l|r|u|d|click> — cursor\n");
  puts("  win <l|r|u|d|desk|open> — top window / redraw / reopen\n");
}

static void cmd_about(void) {
  puts("Diskus kernel P18 - multi-window desktop\n");
}

static void cmd_ui(const char *args) {
  while (*args == ' ')
    args++;
  if (*args == '1')
    desktop_press(0);
  else if (*args == '2')
    desktop_press(1);
  else
    desktop_press(-1);
  mouse_cursor_redraw();
  puts("ui ok\n");
}

static void cmd_m(const char *args) {
  while (*args == ' ')
    args++;
  if (!*args || streq(args, "?")) {
    puts("usage: m l|r|u|d|click\n");
    return;
  }
  if (args[0] == 'l')
    mouse_nudge(-12, 0);
  else if (args[0] == 'r')
    mouse_nudge(12, 0);
  else if (args[0] == 'u')
    mouse_nudge(0, -12);
  else if (args[0] == 'd')
    mouse_nudge(0, 12);
  else if (starts_with(args, "click") || args[0] == 'c')
    mouse_click_at_cursor();
  else {
    puts("usage: m l|r|u|d|click\n");
    return;
  }
  puts("m ok\n");
}

static void cmd_win(const char *args) {
  while (*args == ' ')
    args++;
  if (!*args || streq(args, "desk")) {
    desktop_draw();
    mouse_cursor_redraw();
    puts("desktop ok\n");
    return;
  }
  if (streq(args, "open")) {
    desktop_open_all();
    mouse_cursor_redraw();
    puts("win open\n");
    return;
  }
  if (args[0] == 'l')
    desktop_nudge_win(-12, 0);
  else if (args[0] == 'r')
    desktop_nudge_win(12, 0);
  else if (args[0] == 'u')
    desktop_nudge_win(0, -12);
  else if (args[0] == 'd')
    desktop_nudge_win(0, 12);
  else {
    puts("usage: win l|r|u|d|desk|open\n");
    return;
  }
  mouse_cursor_redraw();
  puts("win ok\n");
}

static void cmd_sysdemo(void) {
  sys_puts("hello from int 0x80 SYS_write\n");
  u32 t = dosyscall(SYS_UPTIME, 0, 0, 0);
  puts("SYS_uptime => ");
  put_u32(t);
  puts(" ticks\n");
}

static void ls_emit(const char *name, u32 size) {
  puts("  ");
  puts(name);
  puts("  ");
  put_u32(size);
  puts("\n");
}

static void cmd_ls(void) {
  puts("ramfs:\n");
  ramfs_list(ls_emit);
}

static void cmd_cat(const char *name) {
  char buf[RAMFS_DATA + 1];
  if (ramfs_read(name, buf, sizeof(buf)) < 0) {
    puts("no such file\n");
    return;
  }
  puts(buf);
  if (buf[0] && buf[0] != '\n') {
    /* ensure newline if file had none */
  }
}

static void cmd_write(char *args) {
  /* write <name> <text...> */
  while (*args == ' ')
    args++;
  if (!*args) {
    puts("usage: write <name> <text>\n");
    return;
  }
  char *name = args;
  while (*args && *args != ' ')
    args++;
  if (*args) {
    *args = 0;
    args++;
  }
  while (*args == ' ')
    args++;
  if (!*args) {
    puts("usage: write <name> <text>\n");
    return;
  }
  int n = ramfs_write(name, args);
  if (n < 0) {
    puts(n == -2 ? "ramfs full\n" : "write fail\n");
    return;
  }
  puts("wrote ");
  put_u32((u32)n);
  puts(" bytes\n");
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
  if (streq(line, "sysdemo")) {
    cmd_sysdemo();
    return;
  }
  if (streq(line, "ui")) {
    cmd_ui("");
    return;
  }
  if (starts_with(line, "ui ")) {
    cmd_ui(line + 3);
    return;
  }
  if (streq(line, "m")) {
    cmd_m("");
    return;
  }
  if (starts_with(line, "m ")) {
    cmd_m(line + 2);
    return;
  }
  if (streq(line, "win")) {
    cmd_win("");
    return;
  }
  if (starts_with(line, "win ")) {
    cmd_win(line + 4);
    return;
  }
  if (streq(line, "run")) {
    run_from_ramfs("hello");
    return;
  }
  if (starts_with(line, "run ")) {
    char *name = line + 4;
    while (*name == ' ')
      name++;
    if (!*name) {
      puts("usage: run [name]\n");
      return;
    }
    run_from_ramfs(name);
    return;
  }
  if (streq(line, "ls")) {
    cmd_ls();
    return;
  }
  if (starts_with(line, "cat ")) {
    cmd_cat(line + 4);
    return;
  }
  if (streq(line, "cat")) {
    puts("usage: cat <name>\n");
    return;
  }
  if (starts_with(line, "write ")) {
    cmd_write(line + 6);
    return;
  }
  if (streq(line, "write")) {
    puts("usage: write <name> <text>\n");
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
  puts("input: serial; win open/l/r | m click | ui 1/2\n");
  puts("> ");

  for (;;) {
    kbd_poll();
    mouse_poll();

    /* Raw PS/2 scancodes: arrows nudge cursor (WSL host mouse often broken) */
    int sc = kbd_pop_scancode();
    if (sc >= 0) {
      if (!(sc & 0x80)) {
        if (sc == 0x4B) { /* left */
          mouse_nudge(-6, 0);
          continue;
        }
        if (sc == 0x4D) { /* right */
          mouse_nudge(6, 0);
          continue;
        }
        if (sc == 0x48) { /* up */
          mouse_nudge(0, -6);
          continue;
        }
        if (sc == 0x50) { /* down */
          mouse_nudge(0, 6);
          continue;
        }
        if (sc == 0x39 && len == 0) { /* space = click */
          mouse_click_at_cursor();
          continue;
        }
      }
      int ch = scancode_to_ascii((u8)sc);
      if (ch >= 0) {
        /* fall through as character */
        sc = -1;
        if (ch == '\r')
          ch = '\n';
        if (ch == '\n') {
          vga_putc('\n');
          buf[len] = 0;
          run_line(buf);
          len = 0;
          puts("> ");
          continue;
        }
        if (ch == '\b') {
          if (len) {
            len--;
            vga_putc('\b');
          }
          continue;
        }
        if (ch >= 32 && ch <= 126 && len + 1 < (int)sizeof(buf)) {
          buf[len++] = (char)ch;
          vga_putc((char)ch);
        }
        continue;
      }
      continue;
    }

    int c = serial_getc();
    if (c < 0) {
      __asm__ volatile("hlt");
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
    if (c == '\b' || c == 127) {
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
  u32 i = 0;
  for (;;) {
    /* spinning pixel in top-right (graphics) */
    u8 c = (u8)(14 + (i % 3));
    gfx_fill_rect(GFX_W - 10, 2, 8, 8, c);
    i++;
  }
}

static void task_exit(void) {
  tasks[current].alive = 0;
  for (;;)
    yield();
}

static void task_spawn(u32 id, const char *name, void (*entry)(void), u32 *stack_top) {
  u32 *sp = stack_top;
  /* IRQ/yield resume: popa then iret → EIP, CS, EFLAGS (same privilege) */
  *--sp = 0x202;          /* EFLAGS (IF=1) */
  *--sp = 0x08;           /* kernel CS */
  *--sp = (u32)entry;     /* EIP */
  *--sp = 0;              /* EAX */
  *--sp = 0;              /* ECX */
  *--sp = 0;              /* EDX */
  *--sp = 0;              /* EBX */
  *--sp = 0;              /* ESP (discarded by popa) */
  *--sp = 0;              /* EBP */
  *--sp = 0;              /* ESI */
  *--sp = 0;              /* EDI <- ESP */
  tasks[id].esp = (u32)sp;
  tasks[id].kstack_top = (u32)stack_top;
  tasks[id].name = name;
  tasks[id].alive = 1;
  tasks[id].user = 0;
  (void)task_exit;
  (void)enter_userland;
  (void)exit_userland;
}

static void task_spawn_user(u32 id, const char *name, u32 entry, u32 uesp, u32 *kstack_top) {
  u32 *sp = kstack_top;
  /* popa + iret with privilege change → SS, ESP, EFLAGS, CS, EIP */
  *--sp = 0x23;           /* SS user */
  *--sp = uesp;           /* ESP user */
  *--sp = 0x202;          /* EFLAGS IF=1 */
  *--sp = 0x1B;           /* CS user */
  *--sp = entry;          /* EIP */
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;
  *--sp = 0;              /* EDI */
  tasks[id].esp = (u32)sp;
  tasks[id].kstack_top = (u32)kstack_top;
  tasks[id].name = name;
  tasks[id].alive = 1;
  tasks[id].user = 1;
  tss_set_esp0((u32)kstack_top);
}

static void tasks_init(void) {
  current = 0;
  task_switches = 0;
  preempt_enabled = 0;
  tasks[0].name = "shell";
  tasks[0].alive = 1;
  tasks[0].user = 0;
  tasks[0].esp = 0;
  tasks[0].kstack_top = (u32)STACK0_TOP;
  tasks[TASK_USER].alive = 0;
  tasks[TASK_USER].user = 1;
  tasks[TASK_USER].kstack_top = (u32)STACK2_TOP;
  (void)STACK0_TOP;
  task_spawn(1, "blink", task_blink, STACK1_TOP);
}

void kmain(void) {
  gfx_clear(0);
  puts("Diskus C\n");
  puts("P18 windows\n");
  desktop_init();
  puts("init ok\n");
  desktop_draw();
  puts("desktop on\n");
  paging_init();
  puts("paging on\n");
  gdt_tss_init();
  TSS_MEM->esp0 = (u32)STACK2_TOP;
  fds_init();
  for (u32 i = 0; i < 256; i++)
    idt_set((u8)i, 0, 0x08, 0);
  idt_set(32, (u32)irq0_stub, 0x08, 0x8E);
  idt_set(33, (u32)irq1_stub, 0x08, 0x8E);
  idt_set(44, (u32)irq12_stub, 0x08, 0x8E); /* IRQ12 */
  idt_set(0x80, (u32)syscall_stub, 0x08, 0xEE);
  idtp.limit = (u16)(256 * sizeof(struct idt_entry) - 1);
  idtp.base = (u32)IDT_MEM;
  load_idt(&idtp);
  pic_remap();
  pit_init(TIMER_HZ);
  ticks = 0;
  if (mouse_init() == 0) {
    puts("mouse on\n");
    mouse_cursor_redraw();
  } else {
    puts("mouse fail\n");
  }
  irq_sti();
  tasks_init();
  ramfs_init();

  puts("diskfs...\n");
  irq_cli();
  int nfiles = diskfs_load();
  irq_sti();
  if (nfiles < 0) {
    puts("diskfs fail ");
    put_u32((u32)(-nfiles));
    puts("\n");
  } else {
    puts("diskfs loaded ");
    put_u32((u32)nfiles);
    puts("\n");
  }

  preempt_enabled = 1;
  u32 sw0 = task_switches;
  while (task_switches - sw0 < 8)
    __asm__ volatile("hlt");
  puts("preempt switches ");
  put_u32(task_switches);
  puts("\n");

  puts("user from disk...\n");
  run_from_ramfs("hello");
  run_from_ramfs("bye");
  run_from_ramfs("cat");

  shell();
}
