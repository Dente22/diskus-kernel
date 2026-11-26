#pragma once

typedef unsigned char u8;

/* Returns 0 on success, <0 if PS/2 mouse init failed. */
int mouse_init(void);
void mouse_irq_handler(void);
void mouse_on_byte(u8 b); /* aux byte arrived via IRQ1 path */
/* Call from idle/shell loop: move cursor, handle click/drag. */
void mouse_poll(void);
void mouse_cursor_redraw(void);
void mouse_get_xy(int *x, int *y);
u8 mouse_get_buttons(void);
/* WSL/QEMU fallback: move/click without host pointer events */
void mouse_nudge(int dx, int dy);
void mouse_click_at_cursor(void);
