/* Diskus P19 — multi-window + dirty-rect drag */
#include "desktop.h"
#include "gfx.h"
#include "ui.h"

enum {
  NWIN = 2,
  TITLE_H = 12,
  CLOSE_S = 10,
  BTN_W = 48,
  BTN_H = 14,
  KIND_APP = 0,
  KIND_HELP = 1,
};

struct win {
  int x, y, w, h;
  u8 alive;
  u8 kind;
  char title[8];
};

#define WINS ((struct win *)0x00068000)
#define ZORDER ((u8 *)0x00068100)
#define DSTATE ((u8 *)0x00068110)
#define GRAB_DX (*(int *)0x00068120)
#define GRAB_DY (*(int *)0x00068124)
#define DRAG_ID (*(int *)0x00068128)

static int in_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static int rects_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void clamp_win(struct win *w) {
  int max_y = GFX_TEXT_ROW0 * 8;
  if (w->x < 0)
    w->x = 0;
  if (w->y < 0)
    w->y = 0;
  if (w->x + w->w > GFX_W)
    w->x = GFX_W - w->w;
  if (w->y + w->h > max_y)
    w->y = max_y - w->h;
  if (w->y < 0)
    w->y = 0;
}

static void wallpaper_rect(int x, int y, int w, int h) {
  int desk_h = GFX_TEXT_ROW0 * 8;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > GFX_W)
    w = GFX_W - x;
  if (y + h > desk_h)
    h = desk_h - y;
  if (w <= 0 || h <= 0)
    return;
  /* stripe pattern matching full desktop_draw */
  gfx_fill_rect(x, y, w, h, 1);
  for (int sy = (y / 20) * 20; sy < y + h; sy += 20) {
    int ry = sy;
    int rh = 10;
    if (ry < y) {
      rh -= (y - ry);
      ry = y;
    }
    if (ry + rh > y + h)
      rh = y + h - ry;
    if (rh > 0)
      gfx_fill_rect(x, ry, w, rh, 2);
  }
}

static void raise_win(int id) {
  int slot = -1;
  for (int i = 0; i < NWIN; i++) {
    if ((int)ZORDER[i] == id) {
      slot = i;
      break;
    }
  }
  if (slot <= 0)
    return;
  for (int i = slot; i > 0; i--)
    ZORDER[i] = ZORDER[i - 1];
  ZORDER[0] = (u8)id;
}

static int topmost_alive(void) {
  for (int i = 0; i < NWIN; i++) {
    int id = (int)ZORDER[i];
    if (WINS[id].alive)
      return id;
  }
  return -1;
}

static int hit_win(int x, int y) {
  for (int i = 0; i < NWIN; i++) {
    int id = (int)ZORDER[i];
    struct win *w = &WINS[id];
    if (!w->alive)
      continue;
    if (in_rect(x, y, w->x, w->y, w->w, w->h))
      return id;
  }
  return -1;
}

static void close_rect(struct win *w, int *cx, int *cy) {
  *cx = w->x + w->w - CLOSE_S - 2;
  *cy = w->y + 1;
}

static void draw_close(struct win *w) {
  int cx, cy;
  close_rect(w, &cx, &cy);
  gfx_fill_rect(cx, cy, CLOSE_S, CLOSE_S, 4);
  gfx_rect(cx, cy, CLOSE_S, CLOSE_S, 15);
  gfx_putpixel(cx + 2, cy + 2, 15);
  gfx_putpixel(cx + 3, cy + 3, 15);
  gfx_putpixel(cx + 4, cy + 4, 15);
  gfx_putpixel(cx + 5, cy + 5, 15);
  gfx_putpixel(cx + 6, cy + 6, 15);
  gfx_putpixel(cx + 6, cy + 2, 15);
  gfx_putpixel(cx + 5, cy + 3, 15);
  gfx_putpixel(cx + 3, cy + 5, 15);
  gfx_putpixel(cx + 2, cy + 6, 15);
}

static void set_title(struct win *w, const char *s) {
  int i = 0;
  while (s[i] && i < 7) {
    w->title[i] = s[i];
    i++;
  }
  w->title[i] = 0;
}

static void draw_one(struct win *w, int focused) {
  u8 title_bg = focused ? (u8)9 : (u8)8;
  int btn_yes = (int)DSTATE[2];
  int btn_no = (int)DSTATE[3];
  ui_panel_title(w->x, w->y, w->w, w->h, w->title, 7, 15, title_bg, 15);
  draw_close(w);

  if (w->kind == KIND_APP) {
    ui_button(w->x + 8, w->y + TITLE_H + 24, BTN_W, BTN_H, "Yes", btn_yes);
    ui_button(w->x + 64, w->y + TITLE_H + 24, BTN_W, BTN_H, "No", btn_no);
  } else {
    gfx_set_cursor((u32)((w->x + 8) / 8), (u32)((w->y + TITLE_H + 16) / 8));
    gfx_puts_bg("z-order", 0, 7);
    gfx_set_cursor((u32)((w->x + 8) / 8), (u32)((w->y + TITLE_H + 28) / 8));
    gfx_puts_bg("x=close", 0, 7);
  }
}

/* Move window with dirty-rect: wallpaper old area, redraw overlaps, paint window. */
static void desktop_move_dirty(int id, int nx, int ny) {
  struct win *w = &WINS[id];
  int ox = w->x, oy = w->y, ow = w->w, oh = w->h;
  u32 save_c, save_r;

  w->x = nx;
  w->y = ny;
  clamp_win(w);
  if (w->x == ox && w->y == oy)
    return;

  gfx_get_cursor(&save_c, &save_r);

  wallpaper_rect(ox, oy, ow, oh);

  /* other windows intersecting old or new rect, bottom → top */
  for (int i = NWIN - 1; i >= 0; i--) {
    int oid = (int)ZORDER[i];
    if (oid == id || !WINS[oid].alive)
      continue;
    struct win *o = &WINS[oid];
    if (rects_overlap(ox, oy, ow, oh, o->x, o->y, o->w, o->h) ||
        rects_overlap(w->x, w->y, w->w, w->h, o->x, o->y, o->w, o->h))
      draw_one(o, 0);
  }

  draw_one(w, ZORDER[0] == (u8)id);

  if (save_r < (u32)GFX_TEXT_ROW0)
    gfx_set_cursor(0, GFX_TEXT_ROW0);
  else
    gfx_set_cursor(save_c, save_r);
}

void desktop_draw(void) {
  u32 save_c, save_r;
  gfx_get_cursor(&save_c, &save_r);

  wallpaper_rect(0, 0, GFX_W, GFX_TEXT_ROW0 * 8);

  for (int i = NWIN - 1; i >= 0; i--) {
    int id = (int)ZORDER[i];
    if (!WINS[id].alive)
      continue;
    clamp_win(&WINS[id]);
    draw_one(&WINS[id], i == 0);
  }

  if (save_r < (u32)GFX_TEXT_ROW0)
    gfx_set_cursor(0, GFX_TEXT_ROW0);
  else
    gfx_set_cursor(save_c, save_r);
}

void desktop_init(void) {
  WINS[0].x = 20;
  WINS[0].y = 8;
  WINS[0].w = 140;
  WINS[0].h = 80;
  WINS[0].alive = 1;
  WINS[0].kind = KIND_APP;
  set_title(&WINS[0], "App");

  WINS[1].x = 100;
  WINS[1].y = 28;
  WINS[1].w = 120;
  WINS[1].h = 70;
  WINS[1].alive = 1;
  WINS[1].kind = KIND_HELP;
  set_title(&WINS[1], "Help");

  ZORDER[0] = 1;
  ZORDER[1] = 0;
  DRAG_ID = -1;
  DSTATE[1] = 0;
  DSTATE[2] = (u8)UI_BTN_FOCUS;
  DSTATE[3] = (u8)UI_BTN_NORMAL;
}

void desktop_open_all(void) {
  for (int i = 0; i < NWIN; i++)
    WINS[i].alive = 1;
  desktop_draw();
}

void desktop_press(int which) {
  if (which == 0) {
    DSTATE[2] = (u8)UI_BTN_PRESSED;
    DSTATE[3] = (u8)UI_BTN_NORMAL;
  } else if (which == 1) {
    DSTATE[3] = (u8)UI_BTN_PRESSED;
    DSTATE[2] = (u8)UI_BTN_NORMAL;
  } else {
    DSTATE[2] = (u8)UI_BTN_FOCUS;
    DSTATE[3] = (u8)UI_BTN_NORMAL;
  }
  desktop_draw();
}

void desktop_nudge_win(int dx, int dy) {
  int id = topmost_alive();
  if (id < 0)
    return;
  desktop_move_dirty(id, WINS[id].x + dx, WINS[id].y + dy);
}

void desktop_pointer(int x, int y, u8 buttons) {
  u8 left = buttons & 1;
  u8 prev_left = DSTATE[1];

  if (left && !prev_left) {
    int id = hit_win(x, y);
    if (id >= 0) {
      raise_win(id);
      struct win *w = &WINS[id];
      int cx, cy;
      close_rect(w, &cx, &cy);
      if (in_rect(x, y, cx, cy, CLOSE_S, CLOSE_S)) {
        w->alive = 0;
        DRAG_ID = -1;
        desktop_draw();
      } else if (in_rect(x, y, w->x, w->y, w->w - CLOSE_S - 4, TITLE_H)) {
        DRAG_ID = id;
        GRAB_DX = x - w->x;
        GRAB_DY = y - w->y;
        desktop_draw(); /* raise chrome */
      } else if (w->kind == KIND_APP) {
        int by = w->y + TITLE_H + 24;
        if (in_rect(x, y, w->x + 8, by, BTN_W, BTN_H))
          desktop_press(0);
        else if (in_rect(x, y, w->x + 64, by, BTN_W, BTN_H))
          desktop_press(1);
        else
          desktop_draw();
      } else {
        desktop_draw();
      }
    }
  } else if (left && DRAG_ID >= 0) {
    desktop_move_dirty(DRAG_ID, x - GRAB_DX, y - GRAB_DY);
  } else if (!left && prev_left) {
    DRAG_ID = -1;
  }

  DSTATE[1] = left;
}
