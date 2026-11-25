#!/usr/bin/env bash
# One-shot setup inside DiskusUbuntu (WSL2): zig + sanity
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZDIR="$ROOT/tools/zig-linux"
VER="${ZIG_VERSION:-0.17.1}"
# tarball name changed across releases; try current layout
URL="https://ziglang.org/download/${VER}/zig-x86_64-linux-${VER}.tar.xz"
URL_ALT="https://ziglang.org/download/${VER}/zig-linux-x86_64-${VER}.tar.xz"

echo "== Diskus VM setup =="
echo "repo: $ROOT"

need_pkg() {
  local p="$1"
  dpkg -s "$p" >/dev/null 2>&1 || {
    echo "Missing package $p — run: sudo apt-get install -y git nasm qemu-system-x86 build-essential curl xz-utils python3"
    exit 1
  }
}
need_pkg nasm
need_pkg qemu-system-x86
command -v git >/dev/null
command -v python3 >/dev/null

if [[ ! -x "$ZDIR/zig" ]]; then
  echo "Downloading Zig $VER ..."
  mkdir -p "$ROOT/tools"
  TMP="$(mktemp -d)"
  if ! curl -fL --retry 3 -o "$TMP/zig.tar.xz" "$URL"; then
    curl -fL --retry 3 -o "$TMP/zig.tar.xz" "$URL_ALT"
  fi
  tar -xJf "$TMP/zig.tar.xz" -C "$TMP"
  rm -rf "$ZDIR"
  mv "$TMP"/zig-*linux* "$ZDIR" 2>/dev/null || mv "$TMP"/zig-linux-* "$ZDIR"
  # if tar extracted a single dir with zig inside
  if [[ ! -x "$ZDIR/zig" ]]; then
    inner="$(find "$TMP" -maxdepth 2 -type f -name zig | head -1)"
    [[ -n "$inner" ]] || { echo "zig binary missing in archive"; exit 1; }
    mkdir -p "$ZDIR"
    cp -a "$(dirname "$inner")/." "$ZDIR/"
  fi
  rm -rf "$TMP"
fi
echo "zig: $($ZDIR/zig version)"

chmod +x "$ROOT/scripts/"*.sh
echo "OK — build: ./scripts/build.sh"
echo "     smoke: ./scripts/run.sh --smoke"
echo "     qemu:  ./scripts/run.sh"
