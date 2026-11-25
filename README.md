# Diskus Kernel

Hobby OS с нуля (BYOX-стиль). **Не** Diskus Desktop / хаб — отдельный учебный kernel.

## Сейчас: P2 — C kernel

Boot грузит kernel с диска → protected mode → `kmain` печатает `Diskus C`.

```powershell
winget install NASM.NASM SoftwareFreedomConservancy.QEMU
# Zig: portable в tools\zig  (или winget install zig.zig)
.\scripts\build.ps1
.\scripts\run.ps1
```

Смоук: `.\scripts\run.ps1 -Smoke` → `Diskus C`. См. `FOCUS.md`.
