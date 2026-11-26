#pragma once

typedef unsigned char u8;
typedef unsigned int u32;

enum { GFX_W = 320, GFX_H = 200, GFX_TEXT_ROW0 = 8 };

void gfx_clear(u8 color);
void gfx_putpixel(int x, int y, u8 color);
void gfx_fill_rect(int x, int y, int w, int h, u8 color);
void gfx_rect(int x, int y, int w, int h, u8 color);
void gfx_putc(char c, u8 fg);
void gfx_puts(const char *s, u8 fg);
void gfx_set_cursor(u32 col, u32 row);
void gfx_clear_text(void);
void gfx_demo(void);
