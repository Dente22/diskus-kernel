#!/usr/bin/env bash
# Run Diskus in QEMU. Use --smoke for serial-only check.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="$ROOT/build/diskus.img"
[[ -f "$IMG" ]] || "$ROOT/scripts/build.sh"

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing: $1" >&2; exit 1; }; }
need qemu-system-i386

echo "QEMU: $(command -v qemu-system-i386)"
echo "Image: $IMG"

if [[ "${1:-}" == "--smoke" ]]; then
  LOG="$ROOT/build/serial.log"
  rm -f "$LOG"
  qemu-system-i386 -drive "format=raw,file=$IMG" -boot order=c -display none \
    -serial "file:$LOG" -no-reboot &
  QPID=$!
  sleep 3
  kill "$QPID" 2>/dev/null || true
  wait "$QPID" 2>/dev/null || true
  sleep 0.3
  TEXT="$(cat "$LOG" 2>/dev/null || true)"
  printf '%s\n' "$TEXT"
  if echo "$TEXT" | grep -q 'P10c' && echo "$TEXT" | grep -q 'diskfs loaded' \
    && echo "$TEXT" | grep -q 'hello from ring3' && echo "$TEXT" | grep -q 'bye from ring3'; then
    echo 'SMOKE OK'
    exit 0
  fi
  echo "SMOKE FAIL - no P10c disk load on serial. Got: $TEXT" >&2
  exit 1
fi

exec qemu-system-i386 -drive "format=raw,file=$IMG" -boot order=c
