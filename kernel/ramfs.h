#pragma once

typedef unsigned int u32;

enum { RAMFS_NAME = 16, RAMFS_DATA = 128, RAMFS_MAX = 8 };

void ramfs_init(void);
int ramfs_write(const char *name, const char *data);
int ramfs_read(const char *name, char *out, u32 out_cap);
void ramfs_list(void (*emit)(const char *name, u32 size));
