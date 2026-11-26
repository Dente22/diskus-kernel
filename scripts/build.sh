#!/usr/bin/env bash
# Build Diskus boot + C kernel image (Linux / WSL)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build"
KERNEL="$ROOT/kernel"
mkdir -p "$OUT"

find_zig() {
  if command -v zig >/dev/null 2>&1; then command -v zig; return; fi
  for d in "$ROOT/tools/zig-linux" "$ROOT/tools/zig" /opt/zig "$HOME/zig"; do
    if [[ -x "$d/zig" ]]; then echo "$d/zig"; return; fi
  done
  echo "zig not found. Run scripts/vm-setup.sh" >&2
  exit 1
}

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing: $1" >&2; exit 1; }; }
need nasm
ZIG="$(find_zig)"
echo "nasm: $(command -v nasm)"
echo "zig:  $ZIG"

nasm -f bin "$ROOT/boot/boot.asm" -o "$OUT/boot.bin"
[[ "$(wc -c < "$OUT/boot.bin")" -eq 512 ]] || { echo "boot.bin must be 512"; exit 1; }

build_user() {
  local asm="$1" bin="$2"
  nasm -f bin "$KERNEL/$asm" -o "$OUT/$bin"
  wc -c < "$OUT/$bin" | tr -d ' '
}

USER_LEN="$(build_user user.asm user.bin)"
USER2_LEN="$(build_user user2.asm user2.bin)"
USER3_LEN="$(build_user user_cat.asm user_cat.bin)"

nasm -f elf32 "$KERNEL/entry.asm" -o "$OUT/entry.o"
nasm -f elf32 "$KERNEL/idt.asm" -o "$OUT/idt.o"
nasm -f elf32 "$KERNEL/userland.asm" -o "$OUT/userland.o"

CFLAGS=(-target x86-freestanding-none -ffreestanding -fno-stack-protector -fno-pic -O2 -I "$KERNEL")
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/main.c" -o "$OUT/main.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/ramfs.c" -o "$OUT/ramfs.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/disk.c" -o "$OUT/disk.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/paging.c" -o "$OUT/paging.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/gfx.c" -o "$OUT/gfx.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/ui.c" -o "$OUT/ui.o"
"$ZIG" cc "${CFLAGS[@]}" -c "$KERNEL/mouse.c" -o "$OUT/mouse.o"

OBJS=("$OUT/entry.o" "$OUT/idt.o" "$OUT/userland.o" "$OUT/main.o" "$OUT/ramfs.o" "$OUT/disk.o" "$OUT/paging.o" "$OUT/gfx.o" "$OUT/ui.o" "$OUT/mouse.o")
if command -v ld.lld >/dev/null 2>&1; then
  ld.lld -m elf_i386 -T "$KERNEL/linker.ld" -o "$OUT/kernel.elf" "${OBJS[@]}"
elif [[ -x "$(dirname "$ZIG")/lld" ]]; then
  "$(dirname "$ZIG")/lld" -m elf_i386 -T "$KERNEL/linker.ld" -o "$OUT/kernel.elf" "${OBJS[@]}"
else
  "$ZIG" ld.lld -m elf_i386 -T "$KERNEL/linker.ld" -o "$OUT/kernel.elf" --oformat=elf "${OBJS[@]}"
fi

"$ZIG" objcopy -O binary "$OUT/kernel.elf" "$OUT/kernel.bin" || \
  "$ZIG" objcopy -I elf32-i386 -O binary "$OUT/kernel.elf" "$OUT/kernel.bin"

KERNEL_SECS=64
PAD=$((KERNEL_SECS * 512))
KSIZE=$(wc -c < "$OUT/kernel.bin" | tr -d ' ')
[[ "$KSIZE" -le "$PAD" ]] || { echo "kernel.bin too large: $KSIZE > $PAD"; exit 1; }
dd if=/dev/zero of="$OUT/kernel.pad" bs=1 count="$PAD" status=none
dd if="$OUT/kernel.bin" of="$OUT/kernel.pad" conv=notrunc status=none

# DKFS: magic + count + files, pad to 2 sectors
python3 - "$OUT" <<'PY'
import struct, pathlib, sys
out = pathlib.Path(sys.argv[1])
hello = (out / "user.bin").read_bytes()
bye = (out / "user2.bin").read_bytes()
catp = (out / "user_cat.bin").read_bytes()

def put(name: str, data: bytes) -> bytes:
    nb = name.encode("ascii")[:15].ljust(16, b"\0")
    return nb + struct.pack("<I", len(data)) + data

blob = b"DKFS" + bytes([3, 0, 0, 0]) + put("hello", hello) + put("bye", bye) + put("cat", catp)
need = max(1024, (len(blob) + 511) // 512 * 512)
blob = blob.ljust(need, b"\0")
(out / "dkfs.bin").write_bytes(blob)
print(len(hello), len(bye), len(catp), len(blob))
PY

cat "$OUT/boot.bin" "$OUT/kernel.pad" "$OUT/dkfs.bin" > "$OUT/diskus.img"
IMG_SIZE=$(wc -c < "$OUT/diskus.img" | tr -d ' ')
echo "OK $OUT/diskus.img ($IMG_SIZE bytes; kernel $KSIZE raw; DKFS user $USER_LEN+$USER2_LEN+$USER3_LEN)"
