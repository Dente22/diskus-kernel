# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**—** фаза не выбрана (P11 закрыт)

## Чеклист
- (пусто)

## Закрыто
- P0–P11 — ✅ … → DKFS → **paging (identity map)**
- VM — ✅ WSL2 `DiskusUbuntu`

## Кандидаты
- **P12** — больше syscalls (файлы из ring3)
- **P13** — user process в планировщике
- или скажи свою идею

## Парковка
- UEFI / Limine / Desktop / Rust / full disk FS / раздельные user pages

## Правило
Каждую фазу дописывать в **`LEARN.md`**.  
После крупного изменения / пачки — **commit + push**.  
Эксперименты — из WSL (`VM.md`).
