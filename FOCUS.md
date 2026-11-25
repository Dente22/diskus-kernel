# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**P0 — Boot sector** — смоук в QEMU → ждём «ок»

## Чеклист
- [x] `boot/boot.asm` — BIOS VGA + COM1
- [x] Сборка → `build/diskus.img` (512 байт)
- [x] `scripts/build.ps1` + `scripts/run.ps1 -Smoke`
- [x] Автосмоук: serial вывел `Diskus`
- [ ] Ты увидел в окне QEMU → «ок»

### Смоук руками
```powershell
.\scripts\run.ps1
```
В окне: `Diskus` / `kernel P0 - boot OK`. Закрой QEMU → напиши «ок».

## Дальше (не трогать до «ок»)
- P1 — second stage / protected mode
- P2 — C kernel + VGA
- P3 — keyboard + simple shell

## Парковка
- UEFI / Limine
- FS
- Связь с Diskus Desktop

## Бэклог
- Rust kernel позже
