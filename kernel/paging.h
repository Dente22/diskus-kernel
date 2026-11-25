#pragma once

/* Identity-map low memory and set CR0.PG. Call once, before relying on high addrs. */
void paging_init(void);
