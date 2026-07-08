#!/usr/bin/env bash

set -euo pipefail

APP="linotepad"

echo "[*] Checking dependencies..."

command -v gcc >/dev/null
command -v pkg-config >/dev/null
pkg-config --exists gtk+-3.0

echo "[*] Compiling ${APP}..."

gcc \
    -Os \
    -flto \
    -s \
    -DNDEBUG \
    -fmerge-all-constants \
    -fPIE \
    -ffunction-sections \
    -fdata-sections \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -Wno-deprecated-declarations \
    "${APP}.c" \
    -o "${APP}" \
    $(pkg-config --cflags --libs gtk+-3.0) \
    -pie \
    -Wl,--gc-sections \
    -Wl,--as-needed \
    -Wl,--build-id=none \
    -Wl,-z,relro \
    -Wl,-z,now

echo "[*] Optimizing binary..."

objcopy --remove-section=.comment "${APP}"

echo "[*] Creating user directories..."

mkdir -p ~/.local/share/icons
mkdir -p ~/.local/share/applications

echo "[*] Installing icon..."

install -Dm644 linotepad.png \
    ~/.local/share/icons/linotepad.png

echo "[*] Installing desktop entry..."

install -Dm644 linotepad.desktop \
    ~/.local/share/applications/linotepad.desktop

echo "[*] Installing binary..."

sudo install -Dm755 "${APP}" /usr/bin/"${APP}"

echo "[*] Refreshing desktop database..."

update-desktop-database ~/.local/share/applications >/dev/null 2>&1 || true

echo
echo "Installation complete!"