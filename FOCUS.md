# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**P1 — Protected mode** — автосмоук OK → окно QEMU → «ок»

## Чеклист
- [x] GDT (null / code / data)
- [x] A20 + switch CR0.PE
- [x] Far jump в 32-bit, печать `Diskus PM`
- [x] Автосмоук serial
- [ ] Ты видишь в окне → «ок»

## Закрыто
- P0 Boot sector (VGA+COM1, QEMU) — ✅

## Дальше
- P2 — C kernel + VGA
- P3 — keyboard + simple shell

## Парковка
- UEFI / Limine
- FS
- Связь с Diskus Desktop

## Бэклог
- Rust kernel позже
