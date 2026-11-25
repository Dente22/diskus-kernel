# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**P2 — C kernel + VGA** — автосмоук OK → окно → «ок»

## Чеклист
- [x] Boot: int 0x13 грузит kernel с диска
- [x] `kernel/` — entry.asm + main.c (VGA 0xB8000)
- [x] Сборка img (boot + kernel) через Zig + NASM
- [x] Автосмоук: `Diskus C` на serial
- [ ] Ты видишь в окне → «ок»

## Закрыто
- P0 Boot sector — ✅
- P1 Protected mode — ✅

## Дальше
- P3 — keyboard + simple shell

## Парковка
- UEFI / Limine
- FS
- Связь с Diskus Desktop

## Бэклог
- Rust kernel позже
