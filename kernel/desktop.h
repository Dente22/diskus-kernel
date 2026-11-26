#pragma once

typedef unsigned char u8;

void desktop_init(void);
void desktop_draw(void);
/* Pointer: buttons bit0=left. Handles drag + button clicks. */
void desktop_pointer(int x, int y, u8 buttons);
void desktop_press(int which); /* 0=yes 1=no -1=reset */
void desktop_nudge_win(int dx, int dy);
