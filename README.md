# Diskus Kernel

Hobby OS с нуля (BYOX-стиль). **Не** Diskus Desktop / хаб — отдельный учебный kernel.

## P0 — Boot

Печатает `Diskus` через BIOS в QEMU.

```powershell
winget install NASM.NASM SoftwareFreedomConservancy.QEMU
.\scripts\build.ps1
.\scripts\run.ps1
```

Смоук: в окне QEMU видно `Diskus` / `kernel P0 — boot OK`.

См. `FOCUS.md`.
