# Diskus — виртуалка для экспериментов

Среда: **WSL2** + дистрибутив **`DiskusUbuntu`** (Ubuntu 24.04).  
На этой машине уже был Hyper-V — VirtualBox с ним конфликтует, поэтому WSL2.

## Вход

Из PowerShell / CMD:

```bat
wsl -d DiskusUbuntu
```

Или из корня репо на Windows:

```bat
wsl -d DiskusUbuntu --cd /mnt/c/Users/desk-kiosk/Desktop/diskus-kernel
```

Репо на Windows сразу видно в Linux как `/mnt/c/Users/desk-kiosk/Desktop/diskus-kernel`.

## Первый setup (один раз)

```bash
cd /mnt/c/Users/desk-kiosk/Desktop/diskus-kernel
./scripts/vm-setup.sh
```

Ставит Zig в `tools/zig-linux/` (nasm/qemu/git уже в образе).

## Сборка и прогон

```bash
./scripts/build.sh
./scripts/run.sh --smoke   # serial check
./scripts/run.sh           # окно QEMU (нужен WSLg или VcXsrv; smoke достаточно без GUI)
```

## Git с основного ПК

Репо: https://github.com/Dente22/diskus-kernel  

```bash
git pull
./scripts/build.sh && ./scripts/run.sh --smoke
```

## Полезно

| Команда | Смысл |
|---------|--------|
| `wsl -l -v` | список дистрибутивов |
| `wsl -d DiskusUbuntu` | войти |
| `wsl --shutdown` | остановить все WSL VM |

Дальнейшие фазы ядра — из этой среды (или с Windows-скриптов, они тоже работают).
