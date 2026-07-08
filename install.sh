#!/usr/bin/env bash
set -e

APP="linotepad"

if ! pkg-config --exists gtk+-3.0 2>/dev/null; then
    # Parrot's own repo ships an outdated libgtk-3-dev (3.24.38) that hard-requires
    # an EXACT matching gir1.2-gtk-3.0 version. If a newer gir1.2-gtk-3.0 (e.g. from
    # Debian testing) is already installed, that can't be satisfied.
    #
    # Fix: pull the gtk3 dev stack from testing instead. Do NOT explicitly name
    # "libgtk-3-0" here — Debian renamed the runtime package to "libgtk-3-0t64"
    # (the 64-bit time_t transition), and only the OLD "libgtk-3-0" name still
    # exists in Parrot's own repo at the outdated version. Explicitly requesting
    # it by that literal name causes apt to fetch the stale Parrot build even
    # under -t testing, which then file-conflicts with libgtk-3-0t64 pulled in
    # by libgtk-3-dev. Letting libgtk-3-dev pull in its own correctly-named
    # runtime dependency avoids the collision entirely.
    #
    # Also: Parrot's VERSION_CODENAME in /etc/os-release (e.g. "echo") does not
    # correspond to a real apt suite name (the actual suite is "lory-updates"),
    # so pinning with -t "$VERSION_CODENAME" silently does nothing. We pin to
    # "testing" explicitly instead.
    if [ -r /etc/os-release ]; then
        . /etc/os-release
    fi

    sudo apt update

    if [ "${ID:-}" = "parrot" ]; then
        sudo apt install -y \
            -t testing \
            gcc \
            pkg-config \
            binutils \
            libgtk-3-dev \
            libatk-bridge2.0-dev \
            libatk1.0-dev \
            libcairo2-dev \
            libegl1-mesa-dev \
            libepoxy-dev \
            libfribidi-dev \
            libgdk-pixbuf-2.0-dev \
            libglib2.0-dev \
            libpango1.0-dev \
            libwayland-dev \
            libxcomposite-dev \
            libxcursor-dev \
            libxdamage-dev \
            libxfixes-dev \
            libxi-dev \
            libxinerama-dev \
            libxkbcommon-dev \
            libxrandr-dev
    else
        sudo apt install -y \
            gcc \
            pkg-config \
            binutils \
            libgtk-3-dev
    fi
fi

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

objcopy --remove-section=.comment "${APP}"

install -Dm644 "${APP}.png" \
    "$HOME/.local/share/icons/${APP}.png"
install -Dm644 "${APP}.desktop" \
    "$HOME/.local/share/applications/${APP}.desktop"

sudo install -Dm755 "${APP}" /usr/bin/"${APP}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
fi

echo "Linotepad installed successfully."
