# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**—** фаза не выбрана (P9 закрыт)

## Чеклист
- (пусто)

## Закрыто
- P0–P9 — ✅ boot → PM → C → shell → IRQ/yield → ramfs → syscalls → ring3 → exec from ramfs

## Кандидаты на следующую фазу
- **P10a** — preemptive: switch из IRQ0 (не только `yield`)
- **P10b** — вторая user-прога в ramfs + `run <name>`
- **P10c** — чтение с «диска» (секторы образа) вместо только RAM-сида

## Парковка
- UEFI / Limine / Desktop / Rust / page tables / disk FS

## Правило
Каждую фазу дописывать в **`LEARN.md`**.
