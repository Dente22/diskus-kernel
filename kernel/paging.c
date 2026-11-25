/* Diskus P11 — 32-bit paging, identity map first 4 MiB */
#include "paging.h"

typedef unsigned int u32;

enum {
  PAGE_PRESENT = 1u << 0,
  PAGE_RW = 1u << 1,
  PAGE_USER = 1u << 2, /* U/S=1 — иначе ring3 → page fault */
  PAGE_SIZE = 4096u,
  PTE_COUNT = 1024u,
  /* Cover kernel, VGA, stacks, TSS, user (0x40000), disk buf */
  IDMAP_PAGES = 1024u, /* 4 MiB */
};

/* Page-aligned fixed RAM (objcopy drops BSS) */
#define PD_MEM ((volatile u32 *)0x00065000)
#define PT_MEM ((volatile u32 *)0x00066000)

void paging_init(void) {
  volatile u32 *pd = PD_MEM;
  volatile u32 *pt = PT_MEM;

  for (u32 i = 0; i < PTE_COUNT; i++)
    pd[i] = 0;
  for (u32 i = 0; i < PTE_COUNT; i++)
    pt[i] = 0;

  /* P11: User on all idmap pages so ring3 can run; tighten later */
  for (u32 i = 0; i < IDMAP_PAGES; i++)
    pt[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW | PAGE_USER;

  /* PDE0 → PT (User so walk from CPL3 is allowed) */
  pd[0] = ((u32)PT_MEM) | PAGE_PRESENT | PAGE_RW | PAGE_USER;

  u32 cr0;
  __asm__ volatile("mov %0, %%cr3" : : "r"((u32)PD_MEM) : "memory");
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000u; /* PG */
  __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
  /* Flush prefetch after enabling paging */
  __asm__ volatile("jmp 1f\n1:");
}
