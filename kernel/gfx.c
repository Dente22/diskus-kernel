/* Diskus P14 — Mode 13h framebuffer (320x200x8) at 0xA0000 */
#include "font8.h"
#include "gfx.h"

static volatile u8 *const FB = (volatile u8 *)0x000A0000;
static u32 cur_col;
static u32 cur_row;
enum { COLS = GFX_W / 8, ROWS = GFX_H / 8 };

void gfx_putpixel(int x, int y, u8 color) {
  if (x < 0 || y < 0 || x >= GFX_W || y >= GFX_H)
    return;
  FB[y * GFX_W + x] = color;
}

void gfx_clear(u8 color) {
  for (u32 i = 0; i < GFX_W * GFX_H; i++)
    FB[i] = color;
  cur_col = 0;
  cur_row = 0;
}

void gfx_fill_rect(int x, int y, int w, int h, u8 color) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      gfx_putpixel(x + i, y + j, color);
}

void gfx_rect(int x, int y, int w, int h, u8 color) {
  for (int i = 0; i < w; i++) {
    gfx_putpixel(x + i, y, color);
    gfx_putpixel(x + i, y + h - 1, color);
  }
  for (int j = 0; j < h; j++) {
    gfx_putpixel(x, y + j, color);
    gfx_putpixel(x + w - 1, y + j, color);
  }
}

void gfx_set_cursor(u32 col, u32 row) {
  cur_col = col;
  cur_row = row;
}

static void gfx_draw_glyph(int x, int y, char ch, u8 fg) {
  if (ch < 32 || ch > 127)
    ch = '?';
  const u8 *g = font8[ch - 32];
  for (int row = 0; row < 8; row++) {
    u8 bits = g[row];
    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col))
        gfx_putpixel(x + col, y + row, fg);
    }
  }
}

void gfx_clear_text(void) {
  /* wipe text rows only — keep demo strip at top */
  gfx_fill_rect(0, GFX_TEXT_ROW0 * 8, GFX_W, GFX_H - GFX_TEXT_ROW0 * 8, 0);
  cur_col = 0;
  cur_row = GFX_TEXT_ROW0;
}

void gfx_putc(char c, u8 fg) {
  if (c == '\n') {
    cur_col = 0;
    cur_row++;
    if (cur_row >= ROWS)
      gfx_clear_text();
    return;
  }
  if (c == '\b') {
    if (cur_col) {
      cur_col--;
      gfx_fill_rect((int)(cur_col * 8), (int)(cur_row * 8), 8, 8, 0);
    }
    return;
  }
  if (c < 32)
    return;
  if (cur_col >= COLS) {
    cur_col = 0;
    cur_row++;
    if (cur_row >= ROWS)
      gfx_clear_text();
  }
  gfx_draw_glyph((int)(cur_col * 8), (int)(cur_row * 8), c, fg);
  cur_col++;
}

void gfx_puts(const char *s, u8 fg) {
  while (*s)
    gfx_putc(*s++, fg);
}

void gfx_demo(void) {
  /* compact strip in rows 0..7 (y 0..63) — text starts at GFX_TEXT_ROW0 */
  for (int y = 0; y < 24; y++)
    for (int x = 0; x < GFX_W; x++)
      gfx_putpixel(x, y, (u8)(1 + (y / 4)));

  gfx_fill_rect(8, 28, 70, 28, 1);    /* blue panel */
  gfx_rect(8, 28, 70, 28, 15);        /* white border */
  gfx_fill_rect(90, 30, 24, 24, 4);   /* red */
  gfx_fill_rect(122, 34, 20, 20, 2);  /* green */
  gfx_fill_rect(152, 28, 24, 28, 14); /* yellow */

  gfx_set_cursor(23, 4);
  gfx_puts("P14 Mode13h", 15);
  gfx_set_cursor(0, GFX_TEXT_ROW0);
}
