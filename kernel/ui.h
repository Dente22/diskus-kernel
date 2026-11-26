#pragma once

#include "gfx.h"

enum {
  UI_BTN_NORMAL = 0,
  UI_BTN_PRESSED = 1,
  UI_BTN_FOCUS = 2,
};

void ui_label(int x, int y, const char *s, u8 fg);
void ui_panel(int x, int y, int w, int h, u8 fill, u8 border);
void ui_panel_title(int x, int y, int w, int h, const char *title, u8 fill, u8 border,
                    u8 title_bg, u8 title_fg);
void ui_button(int x, int y, int w, int h, const char *label, int state);
void ui_demo(void);
void ui_demo_press(int which); /* 0=yes, 1=no — toggle pressed look */
