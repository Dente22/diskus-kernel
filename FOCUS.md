# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**VM — среда для экспериментов** — виртуалка + туда репо/тулзы/QEMU

## Чеклист
- [x] WSL2 вместо VirtualBox (Hyper-V уже был)
- [x] Distro `DiskusUbuntu` (Ubuntu 24.04)
- [x] git, nasm, qemu, zig 0.17.1
- [x] `build.sh` + smoke внутри VM (SMOKE OK)
- [x] `VM.md` + `scripts/vm-setup.sh` / `build.sh` / `run.sh`
- [ ] «ок» → push

## Закрыто
- P0–P10c — ✅

## Дальше
- (после VM) — следующие фазы ядра уже из виртуалки

## Парковка
- UEFI / Limine / Desktop / Rust / page tables / full disk FS

## Правило
Каждую фазу дописывать в **`LEARN.md`**.  
После крупного изменения / пачки обновлений — **commit + push** на GitHub (`origin/main`).
