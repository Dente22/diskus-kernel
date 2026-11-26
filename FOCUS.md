# FOCUS — Diskus Kernel (hobby OS)

## Сейчас
**P14 — Framebuffer** — VGA Mode 13h (320×200), пиксели + простой текст

## Чеклист
- [x] Mode 13h (boot BIOS до PM)
- [x] putpixel / fill / rect на `0xA0000`
- [x] Крошечный шрифт → puts на экране (+ serial)
- [x] Boot-демо картинка; smoke по serial
- [x] LEARN.md P14
- [x] WSL → «ок» → push

## Закрыто
- P0–P13, VM — ✅

## Дальше (визуальный трек)
- P15 — UI kit (кнопка/панель)
- P16 — мышь + клик
- потом desktop

## Парковка
- VBE/HD, UEFI, окна, обои

## Правило
`LEARN.md` + push после пачки. Эксперименты — WSL.
