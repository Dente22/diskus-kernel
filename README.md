# Diskus Kernel

Hobby OS с нуля (BYOX-стиль). **Не** Diskus Desktop / хаб — отдельный учебный kernel.

- **`FOCUS.md`** — что делаем сейчас  
- **`LEARN.md`** — как это работает (учись параллельно по фазам)

## Запуск

```powershell
.\scripts\build.ps1
.\scripts\run.ps1          # окно QEMU
.\scripts\run.ps1 -Smoke   # serial-проверка
```

NASM + QEMU + Zig (`tools\zig` или winget).
