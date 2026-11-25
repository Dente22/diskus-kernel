#pragma once

typedef unsigned int u32;

/* Primary ATA PIO — read sectors into buf (must hold sectors*512). Returns 0 ok. */
int ata_read28(u32 lba, void *buf, u32 sectors);

/* Parse DKFS at LBA 65 into ramfs. Returns files loaded, or <0 on error. */
int diskfs_load(void);
