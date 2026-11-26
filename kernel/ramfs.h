#pragma once

typedef unsigned int u32;

enum { RAMFS_NAME = 16, RAMFS_DATA = 256, RAMFS_MAX = 8 };

void ramfs_init(void);
int ramfs_write(const char *name, const char *data);
int ramfs_put(const char *name, const void *data, u32 len);
int ramfs_read(const char *name, char *out, u32 out_cap);
int ramfs_get(const char *name, void *out, u32 out_cap);
void ramfs_list(void (*emit)(const char *name, u32 size));
