#pragma once

typedef unsigned char u8;

void desktop_init(void);
void desktop_draw(void);
void desktop_pointer(int x, int y, u8 buttons);
void desktop_press(int which); /* Yes/No on focused app window */
void desktop_nudge_win(int dx, int dy); /* topmost alive */
void desktop_open_all(void); /* reopen closed windows */
