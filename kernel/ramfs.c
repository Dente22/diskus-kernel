/* Diskus P9 — tiny RAM filesystem (fixed pool, binary-safe put/get) */
#include "ramfs.h"

typedef unsigned char u8;

struct file {
  char name[RAMFS_NAME];
  u32 len;
  u8 used;
  u8 pad[3];
  char data[RAMFS_DATA];
};

/* Fixed RAM — avoid BSS past loaded image (objcopy drops .bss) */
#define FILES_MEM ((struct file *)0x00063000)

static int streq(const char *a, const char *b) {
  while (*a && *b && *a == *b) {
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static void copy_name(char *dst, const char *src) {
  u32 i = 0;
  while (src[i] && i + 1 < RAMFS_NAME) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
}

static struct file *find(const char *name) {
  struct file *files = FILES_MEM;
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (files[i].used && streq(files[i].name, name))
      return &files[i];
  }
  return 0;
}

static struct file *alloc_slot(void) {
  struct file *files = FILES_MEM;
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (!files[i].used)
      return &files[i];
  }
  return 0;
}

void ramfs_init(void) {
  struct file *files = FILES_MEM;
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    files[i].used = 0;
    files[i].len = 0;
    files[i].name[0] = 0;
  }
  ramfs_write("readme", "Diskus ramfs P9 - hello from RAM\n");
}

int ramfs_put(const char *name, const void *data, u32 len) {
  if (!name || !name[0] || !data)
    return -1;
  if (len > RAMFS_DATA)
    len = RAMFS_DATA;
  struct file *f = find(name);
  if (!f) {
    f = alloc_slot();
    if (!f)
      return -2;
    copy_name(f->name, name);
    f->used = 1;
  }
  const volatile u8 *src = (const volatile u8 *)data;
  volatile u8 *dst = (volatile u8 *)f->data;
  for (u32 i = 0; i < len; i++)
    dst[i] = src[i];
  f->len = len;
  return (int)len;
}

int ramfs_write(const char *name, const char *data) {
  if (!data)
    return -1;
  u32 n = 0;
  while (data[n] && n < RAMFS_DATA)
    n++;
  return ramfs_put(name, data, n);
}

int ramfs_get(const char *name, void *out, u32 out_cap) {
  struct file *f = find(name);
  if (!f || !out || out_cap == 0)
    return -1;
  u32 n = f->len;
  if (n > out_cap)
    n = out_cap;
  const volatile u8 *src = (const volatile u8 *)f->data;
  volatile u8 *dst = (volatile u8 *)out;
  for (u32 i = 0; i < n; i++)
    dst[i] = src[i];
  return (int)n;
}

int ramfs_read(const char *name, char *out, u32 out_cap) {
  if (!out || out_cap == 0)
    return -1;
  int n = ramfs_get(name, out, out_cap - 1);
  if (n < 0)
    return n;
  out[n] = 0;
  return n;
}

void ramfs_list(void (*emit)(const char *name, u32 size)) {
  struct file *files = FILES_MEM;
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (files[i].used)
      emit(files[i].name, files[i].len);
  }
}
