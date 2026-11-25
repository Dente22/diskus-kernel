# Diskus Kernel — как это устроено (учебный журнал)

Читай вместе с кодом. Каждая фаза = один понятный шаг.  
Репозиторий: [Dente22/diskus-kernel](https://github.com/Dente22/diskus-kernel). Активная задача — в `FOCUS.md`.

---

## Карта «железа» (что мы трогаем)

| Штука | Зачем |
|--------|--------|
| **BIOS / QEMU** | Включает машину, грузит первые 512 байт диска в `0x7C00` |
| **Boot sector** | Крошечная программа: догрузить ядро, включить 32-bit |
| **GDT** | Таблица сегментов («весь мир — плоская память») |
| **Protected mode** | Режим CPU с 32-bit и защитой (мы пока без paging) |
| **VGA `0xB8000`** | Текст на экране: слово = символ + цвет |
| **COM1 `0x3F8`** | Serial — удобно для автосмоука без окна |
| **PIC + IDT** | Прерывания: таймер, клавиатура |
| **PIT** | Железяка, которая тикает IRQ0 ~100 раз/сек |

Сборка (Windows): `.\scripts\build.ps1` → `build/diskus.img`  
Запуск: `.\scripts\run.ps1` (окно) или `.\scripts\run.ps1 -Smoke` (serial в файл).

---

## P0 — Boot sector («Diskus» на экране)

**Идея:** BIOS кладёт наш код в `0x7C00`. Мы в **real mode 16-bit** печатаем строку через `int 0x10` и шлём то же на serial.

**Файлы:** `boot/boot.asm` (тогда целиком сектор), сейчас boot уже вырос до P2+.

**Почему 512 байт и `0xAA55`:** BIOS проверяет сигнатуру в конце сектора — иначе не загрузит.

**Чему учит:** «самая первая инструкция после включения».

---

## P1 — Protected mode («Diskus PM»)

**Идея:** из 16-bit перейти в 32-bit.

1. Включить **A20** (иначе память «зациклена» как в 8086).
2. Загрузить **GDT** (`lgdt`): null / code / data на всё адресное пространство.
3. Выставить бит PE в `CR0`, сделать far jump `jmp 0x08:pm_entry` — сменить CS.
4. В 32-bit писать прямо в VGA-память (`mov [0xB8000], …`).

**Чему учит:** без GDT и far jump «просто mov cr0» недостаточно — pipeline/селекторы надо сбросить.

---

## P2 — C kernel (`kmain`)

**Идея:** boot только грузит и прыгает; логика — на C.

1. Boot (`int 0x13`) читает следующие сектора диска → `0x10000`.
2. `kernel/entry.asm` ставит стек, вызывает `kmain`.
3. `kernel/main.c` — freestanding (нет libc): сами пишем в VGA/serial.
4. Сборка: **NASM** (asm) + **Zig cc** (`-target x86-freestanding-none`) + линкер `linker.ld` (`ENTRY(_start)`, база `0x10000`).

**Важно:** `objcopy -O binary` часто **не кладёт BSS в файл**. Большие буферы/стеки задач надёжнее класть в **известную RAM** (у нас стек blink на `0x72000`), либо в `.data` с ненулевым байтом, либо расширять загрузку секторов так, чтобы покрыть весь BSS.

**Чему учит:** разделение boot vs kernel, линкер-скрипт, freestanding.

---

## P3 — Shell + клавиатура (poll)

**Идея:** простой REPL: `help` / `clear` / `about` / `echo`.

- Клавиатура: опрос портов `0x64` / `0x60`, scancode set 1 → ASCII.
- Serial тоже как ввод (для автоматизации).
- Пока **без IRQ** на клавиши — крутим poll (и нельзя `hlt` без работающих прерываний).

**Чему учит:** scancodes, line editing, «ОС как программа с циклом».

---

## P4 — IDT + PIT (`uptime`)

**Идея:** таймер по прерываниям.

1. Заполнить **IDT**, `lidt`.
2. **PIC remap**: IRQ0→int 32, IRQ1→int 33 (не пересекаться с CPU exceptions 0–31).
3. Заглушка `irq0_stub` в asm → `timer_handler` → `ticks++` → EOI на PIC.
4. PIT на ~100 Hz.
5. `sti`, в shell команда `uptime`.

**Чему учит:** зачем remap PIC, EOI, разница poll vs IRQ.

---

## P5 — IRQ1 + cooperative tasks

**Что добавили:**

1. **IRQ1** (`idt.asm` → `kbd_irq_handler`): scancode в ring buffer; shell читает очередь, не сырой poll.
2. **PIC mask `0xFC`:** разрешены IRQ0 (таймер) и IRQ1 (клавиатура).
3. **`yield` + `schedule_switch`:** cooperative switch между `shell` и `blink`.
4. Стек blink на **фиксированном адресе `0x72000`** (не BSS образа — иначе `objcopy` «теряет» память).
5. Команда shell: **`tasks`** (список + счётчик switches).
6. Спиннер `|/-\\` в правом верхнем углу VGA — крутится, пока shell в `yield`.

**Yield:**

```
shell ──yield──► сохранить ESP shell
                 взять ESP blink
blink ──yield──► снова shell
```

Это **cooperative**, не preemptive (таймер пока только `ticks++`, не снимает задачу сам).

**Проверка:** `.\scripts\run.ps1` → `tasks`, смотри спиннер; клавиши должны работать через IRQ.

---

## P6 — Tiny ramfs

**Идея:** «файлы» без диска — пул в RAM.

| Команда | Действие |
|---------|----------|
| `ls` | список файлов и размеры |
| `cat <name>` | прочитать |
| `write <name> <text>` | создать/перезаписать |

**Код:** `kernel/ramfs.c` + `ramfs.h` — до 8 файлов, имя ≤15, данные ≤128 байт.  
При старте сид: `readme`.

**Чему учит:** inode/каталог в миниатюре, почему настоящий FS сложнее (блоки, кэш, диск, права).

**Проверка:**
```
ls
cat readme
write note hello
cat note
```

---

## P7 — Syscalls (`int 0x80`)

**Идея:** программа не пишет в VGA напрямую «как ядро», а просит сервис через прерывание.

| Номер | Имя | Аргументы | Результат |
|------|-----|-----------|-----------|
| 1 | `SYS_write` | `ebx`=указатель, `ecx`=длина | число байт |
| 2 | `SYS_UPTIME` | — | `ticks` в `eax` |

**Поток:**
1. C: `dosyscall` → `int $0x80` (eax/ebx/ecx/edx).
2. IDT[0x80] → `syscall_stub` (asm) → `syscall_dispatch` (C).
3. `iret` назад, ответ в `eax`.

**Код:** `kernel/idt.asm` (`syscall_stub`), `main.c` (`syscall_dispatch`, `sysdemo`).  
Пока всё в ring 0 (учебный шаг). Настоящий user mode (ring 3) — следующая ступень.

**Проверка:** при буте строка `syscall path OK`; в shell — `sysdemo`.

---

## P8 — User ring (ring 3)

**Идея:** код «приложения» работает с меньшими привилегиями; в ядро — только через syscall.

1. **GDT** расширен: user code `0x18` / user data `0x20` (DPL3), плюс **TSS** (селектор `0x28`).
2. **TSS.esp0/ss0** — отдельный стек ядра при прерывании из ring3 (`0x8F000`), **не** тот же что `kmain` (`0x90000`).
3. **`enter_userland`**: `iret` с CS=`0x1B`, SS=`0x23` → прыжок на `0x40000`.
4. User-программа (`kernel/user.asm`) пишет строку через `int 0x80`, читает uptime, делает `SYS_EXIT` (3).
5. Gate `0x80` с **DPL3** (`0xEE`), иначе ring3 получит GP fault.
6. IRQ/syscall stubs переключают `DS` на kernel (`0x10`) — после входа в user `DS` был `0x23`.

**Ловушки, на которых споткнулись:**
- BSS/`kernel_esp_save` после `objcopy` нет в образе → fixed RAM (`0x62200`).
- TSS.esp0 = стек kmain → `int 0x80` затирает сохранённый ESP → нет возврата «back from user».
- User blob копируется в `0x40000` (`user_blob.h` генерит `build.ps1`).

**Код:** `userland.asm`, `user.asm`, GDT/TSS/`enter_userland` в `main.c`, `SYS_EXIT` → `exit_userland`.

**Проверка:** smoke — `hello from ring3` + `back from user`; в shell — `run`.

**Чему учит:** RPL/DPL, зачем отдельный TSS-стек, граница user/kernel.

---

## P9 — User-программа из ramfs

**Идея:** «файл» в RAM → скопировать в адрес user → `iret` в ring3. Как крошечный `exec`.

1. **`ramfs_put` / `ramfs_get`** — запись/чтение с явной длиной (бинарь может содержать `0x00`).
2. При старте: `ramfs_put("hello", user_blob, len)` — сид из встроенного `user.asm`.
3. **`run` / `run <name>`** → `ramfs_get` в `0x40000` → `enter_userland`.
4. Пул файлов в fixed RAM (`0x63000`) — снова ловушка BSS/`objcopy`.

**Код:** `ramfs.c` (`put`/`get`), `main.c` (`run_from_ramfs`).  
Текст по-прежнему через `write`/`cat`; бинарь — через `put` (пока сид ядром).

**Проверка:** при буте `loaded hello …` + `hello from ring3`; `ls` видит `hello`; `run` / `run hello` снова.

**Чему учит:** загрузчик vs FS, почему `strlen` плох для бинаря, путь к настоящему `exec`.

---

## P10a — Preemptive multitasking

**Идея:** задачу снимает не она сама (`yield`), а таймер — ОС отбирает CPU.

1. **IRQ0** после `ticks++` и EOI вызывает `schedule_switch` (если `preempt_enabled` и не ring3).
2. Контекст единый: `pusha` + кадр для **`iret`** (EIP/CS/EFLAGS) — и у IRQ, и у `yield`.
3. **blink** крутится без `yield`; shell в простое делает `hlt` (пробуждение по IRQ).
4. Из ring3 не планируем — иначе затрём user/TSS-стек.

**Код:** `idt.asm` (`irq0_stub`, `yield`), `main.c` (`preempt_enabled`, `task_spawn`).

**Проверка:** при буте `preempt switches N` (N≥8); в углу VGA спиннер без `yield`; `tasks` показывает switches.

**Чему учит:** cooperative vs preemptive, зачем одинаковый кадр стека, почему не preempt’ить user вслепую.

---

## P10b — Вторая user-программа

**Идея:** в ramfs лежит не один бинарь — `run <name>` выбирает какой грузить в `0x40000`.

1. `user.asm` → файл `hello` («hello from ring3»).
2. `user2.asm` → файл `bye` («bye from ring3»).
3. `build.ps1` вшивает оба массива в `user_blob.h`.

**Проверка:** бут гоняет оба; `ls`; `run hello` / `run bye`.

**Чему учит:** один loader — много программ; имя файла ≠ адрес в памяти (все грузятся на `0x40000`).

---

## Словарь

- **Syscall** — официальный вход из «приложения» в ядро.
- **Ring 0 / Ring 3** — уровни привилегий CPU (ядро / user).
- **TSS** — структура для стека ядра при входе из user.
- **Preemptive** — ядро само переключает задачи по таймеру.
- **Cooperative** — задача сама вызывает `yield`.

- **Freestanding** — нет ОС под нами, нет обычной libc.
- **Scancode** — сырой код клавиши с контроллера, не буква.
- **EOI** — «я обработал IRQ», иначе PIC не пришлёт следующий.
- **Ring buffer** — очередь фиксированного размера (head/tail).

---

## Как пользоваться этим файлом

После каждой фазы сюда дописывается: *что добавили*, *зачем*, *где в коде*, *как проверить*.  
Если что-то непонятно — спрашивай по конкретному абзацу или команде shell.
