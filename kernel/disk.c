/* Diskus P10c — ATA PIO LBA28 + tiny DKFS tail on the image */
#include "disk.h"
#include "ramfs.h"

typedef unsigned char u8;
typedef unsigned short u16;

enum {
  ATA_DATA = 0x1F0,
  ATA_SECT = 0x1F2,
  ATA_LBA0 = 0x1F3,
  ATA_LBA1 = 0x1F4,
  ATA_LBA2 = 0x1F5,
  ATA_DRIVE = 0x1F6,
  ATA_CMD = 0x1F7,
  ATA_STATUS = 0x1F7,
  ATA_ALT = 0x3F6,
  ATA_CMD_READ = 0x20,
  DKFS_LBA = 65,
  DKFS_SECS = 2,
};

#define SECTOR_BUF ((volatile u8 *)0x00064000)

static inline u8 inb(u16 port) {
  u8 v;
  __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static inline void outb(u16 port, u8 v) {
  __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

static inline u16 inw(u16 port) {
  u16 v;
  __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}

static void ata_delay(void) {
  inb(ATA_ALT);
  inb(ATA_ALT);
  inb(ATA_ALT);
  inb(ATA_ALT);
}

static int ata_wait(u8 mask, u8 val) {
  for (u32 i = 0; i < 500000; i++) {
    u8 st = inb(ATA_STATUS);
    if (st & 0x01)
      return -1;
    if ((st & mask) == val)
      return 0;
  }
  return -2;
}

int ata_read28(u32 lba, void *buf, u32 sectors) {
  if (!buf || sectors == 0 || sectors > 4)
    return -1;

  outb(ATA_ALT, 0x02); /* nIEN: poll, no IRQ14 */
  ata_delay();
  if (ata_wait(0x80, 0) < 0)
    return -2;

  outb(ATA_DRIVE, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
  ata_delay();
  outb(ATA_SECT, (u8)sectors);
  outb(ATA_LBA0, (u8)(lba & 0xFF));
  outb(ATA_LBA1, (u8)((lba >> 8) & 0xFF));
  outb(ATA_LBA2, (u8)((lba >> 16) & 0xFF));
  outb(ATA_CMD, ATA_CMD_READ);
  ata_delay();

  volatile u8 *dst = (volatile u8 *)buf;
  for (u32 s = 0; s < sectors; s++) {
    if (ata_wait(0x80, 0) < 0)
      return -3;
    if (ata_wait(0x08, 0x08) < 0)
      return -4;
    for (u32 i = 0; i < 256; i++) {
      u16 w = inw(ATA_DATA);
      *dst++ = (u8)(w & 0xFF);
      *dst++ = (u8)(w >> 8);
    }
  }
  return 0;
}

int diskfs_load(void) {
  if (ata_read28(DKFS_LBA, (void *)SECTOR_BUF, DKFS_SECS) != 0)
    return -1;

  /* volatile: -O2 must not assume RAM unread after PIO */
  const volatile u8 *p = SECTOR_BUF;
  const volatile u8 *end = p + DKFS_SECS * 512;
  if (p[0] != 'D' || p[1] != 'K' || p[2] != 'F' || p[3] != 'S')
    return -2;
  u8 count = p[4];
  p += 8;
  int loaded = 0;
  for (u8 i = 0; i < count; i++) {
    if (p + 20 > end)
      return -3;
    char name[16];
    for (u32 n = 0; n < 15; n++)
      name[n] = (char)p[n];
    name[15] = 0;
    p += 16;
    u32 len = (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
    p += 4;
    if (len > RAMFS_DATA || p + len > end)
      return -4;
    if (name[0]) {
      /* copy to temp so ramfs_put sees a stable buffer */
      u8 tmp[RAMFS_DATA];
      for (u32 j = 0; j < len; j++)
        tmp[j] = p[j];
      if (ramfs_put(name, tmp, len) >= 0)
        loaded++;
    }
    p += len;
  }
  return loaded;
}
