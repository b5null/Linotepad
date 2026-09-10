#!/usr/bin/env bash
set -euo pipefail

APP=linotepad
SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PREFIX=${PREFIX:-"$HOME/.local"}
DESTDIR=${DESTDIR:-}

if [[ $# -gt 0 ]]; then
    if [[ $# -eq 1 && $1 == --help ]]; then
        cat <<'HELP'
Usage: ./install.sh
       PREFIX=/usr/local ./install.sh
       PREFIX=/usr DESTDIR=/tmp/package ./install.sh

Build a stripped, dynamically linked GTK3 executable and install it, its icon,
and its desktop launcher. Defaults to PREFIX=$HOME/.local; no sudo is invoked.
DESTDIR stages installation without changing the launcher's final binary path.
Required tools: gcc, pkg-config, objcopy, install, mktemp; GTK3 development files.
HELP
        exit 0
    fi
    printf 'Unexpected argument. Use --help for usage.\n' >&2
    exit 2
fi

if [[ $PREFIX != /* || ( -n $DESTDIR && $DESTDIR != /* ) ]]; then
    printf 'PREFIX and nonempty DESTDIR must be absolute paths.\n' >&2
    exit 2
fi
if [[ $PREFIX == *$'\n'* || $PREFIX == *$'\r'* || $PREFIX == *$'\t'* ]]; then
    printf 'PREFIX must not contain tabs or newlines.\n' >&2
    exit 2
fi
PREFIX=${PREFIX%/}
DESTDIR=${DESTDIR%/}

for tool in gcc pkg-config objcopy install mktemp; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing required tool: %s\nInstall your distribution\x27s build tools and GTK3 development package, then rerun.\n' "$tool" >&2
        exit 1
    fi
done
if ! pkg-config --exists gtk+-3.0; then
    printf 'GTK3 development files are missing.\nDebian/Ubuntu: sudo apt install gcc pkg-config binutils libgtk-3-dev\nUse packages matching your distribution and release.\n' >&2
    exit 1
fi
for file in "$APP.c" "$APP.svg" "$APP.desktop"; do
    if [[ ! -r $SOURCE_DIR/$file ]]; then
        printf 'Missing source file: %s\n' "$SOURCE_DIR/$file" >&2
        exit 1
    fi
done

BUILD_DIR=$(mktemp -d)
trap 'rm -rf -- "$BUILD_DIR"' EXIT

# Dynamic GTK linking keeps the executable small; GTK is supplied by the system.
# pkg-config emits compiler arguments, intentionally split without shell globbing.
set -f
gcc \
    -Os -flto -s -DNDEBUG \
    -fmerge-all-constants -fPIE \
    -ffunction-sections -fdata-sections \
    -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -Wno-deprecated-declarations \
    "$SOURCE_DIR/$APP.c" -o "$BUILD_DIR/$APP" \
    $(pkg-config --cflags --libs gtk+-3.0) \
    -pie -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none \
    -Wl,-z,relro -Wl,-z,now
set +f
objcopy --remove-section=.comment "$BUILD_DIR/$APP"

# Escape both layers of desktop-entry syntax: Exec quoting, then string escaping.
EXEC_PATH="$PREFIX/bin/$APP"
EXEC_PATH=${EXEC_PATH//\\/\\\\}
EXEC_PATH=${EXEC_PATH//\"/\\\"}
EXEC_PATH=${EXEC_PATH//\$/\\\$}
EXEC_PATH=${EXEC_PATH//\`/\\\`}
EXEC_PATH=${EXEC_PATH//%/%%}
EXEC_PATH=${EXEC_PATH//\\/\\\\}
while IFS= read -r line || [[ -n $line ]]; do
    if [[ $line == Exec=* ]]; then
        printf 'Exec="%s" %%F\n' "$EXEC_PATH"
    else
        printf '%s\n' "$line"
    fi
done < "$SOURCE_DIR/$APP.desktop" > "$BUILD_DIR/$APP.desktop"

INSTALL_ROOT="$DESTDIR$PREFIX"
install -Dm755 "$BUILD_DIR/$APP" "$INSTALL_ROOT/bin/$APP"
install -Dm644 "$SOURCE_DIR/$APP.svg" \
    "$INSTALL_ROOT/share/icons/hicolor/scalable/apps/$APP.svg"
install -Dm644 "$BUILD_DIR/$APP.desktop" \
    "$INSTALL_ROOT/share/applications/$APP.desktop"

# Staged packages must not update caches on the build host.
if [[ -z $DESTDIR ]]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$INSTALL_ROOT/share/applications" ||
            printf 'Warning: desktop database refresh failed.\n' >&2
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache --force --ignore-theme-index \
            "$INSTALL_ROOT/share/icons/hicolor" ||
            printf 'Warning: icon cache refresh failed.\n' >&2
    fi
fi
printf 'Installed Linotepad into %s\n' "$INSTALL_ROOT"
if [[ -z $DESTDIR && :$PATH: != *":$PREFIX/bin:"* ]]; then
    printf 'To run linotepad by name in a terminal, add %s/bin to PATH.\n' "$PREFIX"
fi
