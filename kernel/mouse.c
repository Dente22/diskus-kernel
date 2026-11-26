/* Diskus P16/P17 — PS/2 mouse + save-under cursor on Mode 13h */
#include "desktop.h"
#include "gfx.h"
#include "mouse.h"

typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char i8;

enum { PS2_DATA = 0x60, PS2_STATUS = 0x64, PS2_CMD = 0x64 };
enum { CUR_W = 8, CUR_H = 12 };

static inline u8 inb(u16 port) {
  u8 v;
  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static inline void outb(u16 port, u8 v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static int ps2_wait_write(void) {
  for (u32 i = 0; i < 100000; i++)
    if (!(inb(PS2_STATUS) & 0x02))
      return 0;
  return -1;
}

static int ps2_wait_read(void) {
  for (u32 i = 0; i < 100000; i++)
    if (inb(PS2_STATUS) & 0x01)
      return 0;
  return -1;
}

static void ps2_flush(void) {
  for (u32 i = 0; i < 64; i++) {
    if (!(inb(PS2_STATUS) & 0x01))
      break;
    (void)inb(PS2_DATA);
  }
}

static int mouse_write(u8 val) {
  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_CMD, 0xD4);
  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_DATA, val);
  return 0;
}

static int mouse_read(u8 *out) {
  if (ps2_wait_read() < 0)
    return -1;
  *out = inb(PS2_DATA);
  return 0;
}

static volatile int mx = 100;
static volatile int my = 45;
static volatile u8 mbuttons;
static volatile u8 mcycle;
static volatile u8 mpacket[3];
static volatile u8 mdirty;
static volatile u8 mready;

static u8 under[CUR_W * CUR_H];
static int cur_x = -1, cur_y = -1;
static u8 cur_shown;
static u8 last_btns;

static const u8 CURSOR[CUR_H][CUR_W] = {
    {1, 0, 0, 0, 0, 0, 0, 0}, {1, 1, 0, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0},
    {1, 1, 1, 1, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 0, 0, 0}, {1, 1, 1, 1, 1, 1, 0, 0},
    {1, 1, 1, 1, 1, 1, 1, 0}, {1, 1, 1, 1, 1, 0, 0, 0}, {1, 0, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 0, 0}, {0, 0, 0, 0, 1, 1, 0, 0}, {0, 0, 0, 0, 0, 1, 1, 0},
};

static void cursor_hide(void) {
  if (!cur_shown)
    return;
  for (int j = 0; j < CUR_H; j++) {
    for (int i = 0; i < CUR_W; i++) {
      int x = cur_x + i;
      int y = cur_y + j;
      if (x >= 0 && y >= 0 && x < GFX_W && y < GFX_H)
        gfx_putpixel(x, y, under[j * CUR_W + i]);
    }
  }
  cur_shown = 0;
}

static void cursor_show(int x, int y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= GFX_W)
    x = GFX_W - 1;
  if (y >= GFX_H)
    y = GFX_H - 1;
  cursor_hide();
  cur_x = x;
  cur_y = y;
  for (int j = 0; j < CUR_H; j++) {
    for (int i = 0; i < CUR_W; i++) {
      int px = x + i;
      int py = y + j;
      if (px >= 0 && py >= 0 && px < GFX_W && py < GFX_H) {
        u8 c = *((volatile u8 *)0xA0000 + py * GFX_W + px);
        under[j * CUR_W + i] = c;
        if (CURSOR[j][i])
          gfx_putpixel(px, py, 15);
      }
    }
  }
  cur_shown = 1;
}

void mouse_cursor_redraw(void) {
  if (!mready)
    return;
  cur_shown = 0;
  cursor_show(mx, my);
}

static void mouse_handle_byte(u8 b) {
  if (mcycle == 0 && !(b & 0x08))
    return;
  mpacket[mcycle++] = b;
  if (mcycle < 3)
    return;
  mcycle = 0;

  u8 flags = mpacket[0];
  if (flags & 0xC0)
    return;

  int dx = (int)(i8)mpacket[1];
  int dy = (int)(i8)mpacket[2];

  int nx = mx + dx;
  int ny = my - dy;
  if (nx < 0)
    nx = 0;
  if (ny < 0)
    ny = 0;
  if (nx >= GFX_W)
    nx = GFX_W - 1;
  if (ny >= GFX_H)
    ny = GFX_H - 1;
  mx = nx;
  my = ny;
  mbuttons = flags & 0x07;
  mdirty = 1;
}

void mouse_on_byte(u8 b) { mouse_handle_byte(b); }

int mouse_init(void) {
  u8 v;

  /*
   * Minimal init — do NOT disable the keyboard port (0xAD).
   * QEMU: bits 4/5 of config are clock-disable; clear them, set IRQ1+IRQ12.
   */
  ps2_flush();

  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_CMD, 0xA8);

  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_CMD, 0x20);
  if (mouse_read(&v) < 0)
    return -1;
  v = (u8)((v | 0x03) & (u8)~0x30);
  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_CMD, 0x60);
  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_DATA, v);

  if (ps2_wait_write() < 0)
    return -1;
  outb(PS2_CMD, 0xAE);

  if (mouse_write(0xFF) < 0)
    return -1;
  if (mouse_read(&v) < 0)
    return -1;
  if (mouse_read(&v) < 0)
    return -1;
  if (mouse_read(&v) < 0)
    return -1;

  if (mouse_write(0xF6) < 0)
    return -1;
  if (mouse_read(&v) < 0)
    return -1;

  if (mouse_write(0xF4) < 0)
    return -1;
  if (mouse_read(&v) < 0)
    return -1;

  ps2_flush();
  mcycle = 0;
  mdirty = 1;
  mready = 1;
  mx = 100;
  my = 45;
  last_btns = 0;
  return 0;
}

void mouse_irq_handler(void) {
  for (u32 n = 0; n < 16; n++) {
    u8 st = inb(PS2_STATUS);
    if (!(st & 0x01))
      break;
    u8 b = inb(PS2_DATA);
    if (st & 0x20)
      mouse_handle_byte(b);
    else {
      extern void kbd_push_scancode(u8 sc);
      kbd_push_scancode(b);
    }
  }
}

void mouse_get_xy(int *x, int *y) {
  if (x)
    *x = mx;
  if (y)
    *y = my;
}

u8 mouse_get_buttons(void) { return mbuttons; }

void mouse_poll(void) {
  /* Bytes drained by kbd_poll / IRQs. Here: cursor + desktop pointer. */
  if (!mready)
    return;
  if (!mdirty && mbuttons == last_btns)
    return;
  mdirty = 0;
  last_btns = mbuttons;
  cursor_hide();
  desktop_pointer(mx, my, mbuttons);
  cursor_show(mx, my);
}

void mouse_nudge(int dx, int dy) {
  if (!mready)
    return;
  int nx = mx + dx;
  int ny = my + dy;
  if (nx < 0)
    nx = 0;
  if (ny < 0)
    ny = 0;
  if (nx >= GFX_W)
    nx = GFX_W - 1;
  if (ny >= GFX_H)
    ny = GFX_H - 1;
  mx = nx;
  my = ny;
  mdirty = 1;
  mouse_poll();
}

void mouse_click_at_cursor(void) {
  if (!mready)
    return;
  cursor_hide();
  /* synthetic press + release at cursor */
  desktop_pointer(mx, my, 1);
  desktop_pointer(mx, my, 0);
  cursor_show(mx, my);
}
