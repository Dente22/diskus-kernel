/* Diskus P17 — single draggable window on a flat desktop */
#include "desktop.h"
#include "gfx.h"
#include "ui.h"

enum {
  TITLE_H = 12,
  WIN_W = 180,
  WIN_H = 88,
  BTN_W = 56,
  BTN_H = 16,
};

static int win_x = 40;
static int win_y = 16;
static int dragging;
static int grab_dx, grab_dy;
static u8 prev_left;
static int btn_yes = UI_BTN_FOCUS;
static int btn_no = UI_BTN_NORMAL;

static int btn_yes_x(void) { return win_x + 12; }
static int btn_yes_y(void) { return win_y + TITLE_H + 28; }
static int btn_no_x(void) { return win_x + 76; }
static int btn_no_y(void) { return win_y + TITLE_H + 28; }

static void clamp_win(void) {
  if (win_x < 0)
    win_x = 0;
  if (win_y < 0)
    win_y = 0;
  if (win_x + WIN_W > GFX_W)
    win_x = GFX_W - WIN_W;
  if (win_y + WIN_H > GFX_TEXT_ROW0 * 8)
    win_y = GFX_TEXT_ROW0 * 8 - WIN_H;
  if (win_y < 0)
    win_y = 0;
}

static int in_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void desktop_draw(void) {
  u32 save_c, save_r;
  gfx_get_cursor(&save_c, &save_r);

  /* wallpaper */
  for (int y = 0; y < GFX_TEXT_ROW0 * 8; y++)
    for (int x = 0; x < GFX_W; x++)
      gfx_putpixel(x, y, (u8)(1 + ((x / 40 + y / 20) & 1)));

  clamp_win();
  ui_panel_title(win_x, win_y, WIN_W, WIN_H, "P17 desktop", 7, 15, 9, 15);

  /* client hint */
  gfx_set_cursor((u32)((win_x + 8) / 8), (u32)((win_y + TITLE_H + 6) / 8));
  gfx_puts_bg("drag title", 0, 7);

  ui_button(btn_yes_x(), btn_yes_y(), BTN_W, BTN_H, "Yes", btn_yes);
  ui_button(btn_no_x(), btn_no_y(), BTN_W, BTN_H, "No", btn_no);

  if (save_r < (u32)GFX_TEXT_ROW0)
    gfx_set_cursor(0, GFX_TEXT_ROW0);
  else
    gfx_set_cursor(save_c, save_r);
}

void desktop_init(void) {
  win_x = 40;
  win_y = 16;
  dragging = 0;
  prev_left = 0;
  btn_yes = UI_BTN_FOCUS;
  btn_no = UI_BTN_NORMAL;
  desktop_draw();
}

void desktop_press(int which) {
  if (which == 0) {
    btn_yes = UI_BTN_PRESSED;
    btn_no = UI_BTN_NORMAL;
  } else if (which == 1) {
    btn_no = UI_BTN_PRESSED;
    btn_yes = UI_BTN_NORMAL;
  } else {
    btn_yes = UI_BTN_FOCUS;
    btn_no = UI_BTN_NORMAL;
  }
  desktop_draw();
}

void desktop_nudge_win(int dx, int dy) {
  win_x += dx;
  win_y += dy;
  clamp_win();
  desktop_draw();
}

void desktop_pointer(int x, int y, u8 buttons) {
  u8 left = buttons & 1;

  if (left && !prev_left) {
    /* press */
    if (in_rect(x, y, win_x, win_y, WIN_W, TITLE_H)) {
      dragging = 1;
      grab_dx = x - win_x;
      grab_dy = y - win_y;
    } else if (in_rect(x, y, btn_yes_x(), btn_yes_y(), BTN_W, BTN_H)) {
      desktop_press(0);
    } else if (in_rect(x, y, btn_no_x(), btn_no_y(), BTN_W, BTN_H)) {
      desktop_press(1);
    }
  } else if (left && dragging) {
    int nx = x - grab_dx;
    int ny = y - grab_dy;
    if (nx != win_x || ny != win_y) {
      win_x = nx;
      win_y = ny;
      clamp_win();
      desktop_draw();
    }
  } else if (!left && prev_left) {
    dragging = 0;
  }

  prev_left = left;
}
