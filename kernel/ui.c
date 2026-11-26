/* Diskus P15 — minimal UI kit on Mode 13h */
#include "ui.h"

static int btn_yes_state = UI_BTN_FOCUS;
static int btn_no_state = UI_BTN_NORMAL;

static u32 strlen_u(const char *s) {
  u32 n = 0;
  while (s[n])
    n++;
  return n;
}

void ui_label(int x, int y, const char *s, u8 fg) {
  gfx_set_cursor((u32)(x / 8), (u32)(y / 8));
  gfx_puts(s, fg);
}

void ui_panel(int x, int y, int w, int h, u8 fill, u8 border) {
  gfx_fill_rect(x, y, w, h, fill);
  gfx_rect(x, y, w, h, border);
}

void ui_panel_title(int x, int y, int w, int h, const char *title, u8 fill, u8 border,
                    u8 title_bg, u8 title_fg) {
  ui_panel(x, y, w, h, fill, border);
  gfx_fill_rect(x + 1, y + 1, w - 2, 10, title_bg);
  gfx_rect(x, y, w, h, border);
  if (title && title[0]) {
    gfx_set_cursor((u32)((x + 4) / 8), (u32)((y + 2) / 8));
    gfx_puts(title, title_fg);
  }
}

void ui_button(int x, int y, int w, int h, const char *label, int state) {
  u8 fill = 7;
  u8 border = 15;
  u8 text = 0;
  int ox = 0, oy = 0;
  if (state == UI_BTN_PRESSED) {
    fill = 8;
    border = 0;
    ox = 1;
    oy = 1;
  } else if (state == UI_BTN_FOCUS) {
    fill = 11;
    border = 15;
  }
  gfx_fill_rect(x, y, w, h, fill);
  gfx_rect(x, y, w, h, border);
  if (state != UI_BTN_PRESSED) {
    gfx_fill_rect(x + 1, y + 1, w - 2, 1, 15);
    gfx_fill_rect(x + 1, y + 1, 1, h - 2, 15);
    gfx_fill_rect(x + 1, y + h - 2, w - 2, 1, 8);
    gfx_fill_rect(x + w - 2, y + 1, 1, h - 2, 8);
  }
  if (label) {
    u32 len = strlen_u(label);
    int tx = x + (w - (int)len * 8) / 2 + ox;
    int ty = y + (h - 8) / 2 + oy;
    if (tx < x + 2)
      tx = x + 2;
    if (ty < y + 2)
      ty = y + 2;
    gfx_set_cursor((u32)(tx / 8), (u32)(ty / 8));
    gfx_puts(label, text);
  }
}

void ui_demo(void) {
  for (int y = 0; y < 20; y++)
    for (int x = 0; x < GFX_W; x++)
      gfx_putpixel(x, y, (u8)(1 + (y / 4)));

  ui_panel_title(4, 22, 200, 50, "P15 UI kit", 1, 15, 9, 15);
  ui_button(12, 38, 56, 16, "Yes", btn_yes_state);
  ui_button(76, 38, 56, 16, "No", btn_no_state);

  ui_panel(210, 22, 106, 50, 0, 7);
  gfx_set_cursor(27, 3);
  gfx_puts("1=Yes 2=No", 14);
  gfx_set_cursor(27, 4);
  gfx_puts("ui=redraw", 7);

  gfx_set_cursor(0, GFX_TEXT_ROW0);
}

void ui_demo_press(int which) {
  if (which == 0) {
    btn_yes_state = UI_BTN_PRESSED;
    btn_no_state = UI_BTN_NORMAL;
  } else if (which == 1) {
    btn_no_state = UI_BTN_PRESSED;
    btn_yes_state = UI_BTN_NORMAL;
  } else {
    btn_yes_state = UI_BTN_FOCUS;
    btn_no_state = UI_BTN_NORMAL;
  }
  ui_demo();
}
