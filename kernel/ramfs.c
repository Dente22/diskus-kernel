/* Diskus P6 — tiny RAM filesystem (fixed pool, no disk) */
#include "ramfs.h"

typedef unsigned char u8;

struct file {
  char name[RAMFS_NAME];
  u32 len;
  u8 used;
  char data[RAMFS_DATA];
};

/* .data with non-zero so it lands in the loaded image */
static struct file files[RAMFS_MAX] = {{.name = {1}}};

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
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (files[i].used && streq(files[i].name, name))
      return &files[i];
  }
  return 0;
}

static struct file *alloc_slot(void) {
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (!files[i].used)
      return &files[i];
  }
  return 0;
}

void ramfs_init(void) {
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    files[i].used = 0;
    files[i].len = 0;
    files[i].name[0] = 0;
  }
  ramfs_write("readme", "Diskus ramfs P6 - hello from RAM\n");
}

int ramfs_write(const char *name, const char *data) {
  if (!name || !name[0] || !data)
    return -1;
  struct file *f = find(name);
  if (!f) {
    f = alloc_slot();
    if (!f)
      return -2;
    copy_name(f->name, name);
    f->used = 1;
  }
  u32 i = 0;
  while (data[i] && i < RAMFS_DATA) {
    f->data[i] = data[i];
    i++;
  }
  f->len = i;
  return (int)i;
}

int ramfs_read(const char *name, char *out, u32 out_cap) {
  struct file *f = find(name);
  if (!f || out_cap == 0)
    return -1;
  u32 n = f->len;
  if (n >= out_cap)
    n = out_cap - 1;
  for (u32 i = 0; i < n; i++)
    out[i] = f->data[i];
  out[n] = 0;
  return (int)n;
}

void ramfs_list(void (*emit)(const char *name, u32 size)) {
  for (u32 i = 0; i < RAMFS_MAX; i++) {
    if (files[i].used)
      emit(files[i].name, files[i].len);
  }
}
