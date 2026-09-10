# Linotepad

![Linotepad icon](linotepad.svg)

A lightweight **Notepad-style text editor for Linux**, built with GTK3.

Linotepad is a from-scratch reimplementation inspired by Microsoft's classic **Notepad**. Since the original application depends on Win32 APIs (RichEdit, HMENU, Common Dialogs, etc.), this project recreates the same look and core functionality using native GTK3 widgets on Linux.

The optimized x86-64 executable measured **30,576 bytes (29.9 KiB)** with the build flags below. Size varies by compiler and platform; GTK3 and its shared dependencies are not included in that figure.

---

## Features

- New Window
- Open
- Save
- Save As
- Print support
- Undo / Redo, including grouped Replace All
- Cut / Copy / Paste / Delete
- Select All
- Insert current Time/Date
- Find
- Find Next
- Replace
- Replace All
- Go To Line
- Word Wrap
- Font selection
- Status bar with Line / Column position and zoom percentage
- Zoom using keyboard shortcuts, Ctrl + mouse wheel, or Ctrl + smooth touchpad scrolling
- UTF-8 input validation
- Multiple command-line files, each in its own window
- Native GTK3 dialogs
- Desktop launcher and icon installer

---

## Dependencies

Debian / Ubuntu:

```bash
sudo apt install gcc pkg-config libgtk-3-dev binutils
```

---

## Building

Compile manually:

```bash
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
    linotepad.c \
    -o linotepad \
    $(pkg-config --cflags --libs gtk+-3.0) \
    -pie \
    -Wl,--gc-sections \
    -Wl,--as-needed \
    -Wl,--build-id=none \
    -Wl,-z,relro \
    -Wl,-z,now

objcopy --remove-section=.comment linotepad
```

---

## Installation

The installer builds a stripped, dynamically linked executable with the same
size-focused flags shown above. GTK3 remains a system dependency; the executable
size does not include GTK or its dependencies. It does not install packages or
invoke `sudo` automatically.

Install the dependencies above, then run from the repository directory:

```bash
./install.sh
```

The script also works from another directory when invoked by its full path.

By default, this installs:

- Binary: `~/.local/bin/linotepad`
- Icon: `~/.local/share/icons/hicolor/scalable/apps/linotepad.svg`
- Launcher: `~/.local/share/applications/linotepad.desktop`

The launcher uses the absolute installed binary path. Add `~/.local/bin` to your
shell's `PATH` if needed to run `linotepad` by name. Compilation happens in a
temporary directory, leaving any existing repository binary untouched.

For an optional system-wide installation:

```bash
sudo env PREFIX=/usr/local ./install.sh
```

For packaging or inspecting the installation without installing into the live
system:

```bash
PREFIX=/usr DESTDIR=/tmp/linotepad-package ./install.sh
```

`PREFIX` selects the final installation prefix; `DESTDIR` is an optional staging
root. Both must be absolute paths. Staged installs skip desktop and icon cache
updates. The installed launcher supports selecting multiple files, opening each
in its own window.

---

## Running

```bash
linotepad
```

Open one or more files, each in its own window:

```bash
linotepad myfile.txt notes.txt
```

To try a manually built executable before installation:

```bash
./linotepad
```

---

## Keyboard Shortcuts

| Action | Shortcut |
|---------|----------|
| New Window | `Ctrl + N` |
| Close Window | `Ctrl + W` |
| Open | `Ctrl + O` |
| Save | `Ctrl + S` |
| Save As | `Ctrl + Shift + S` |
| Print | `Ctrl + P` |
| Undo / Redo | `Ctrl + Z` / `Ctrl + Y` |
| Cut | `Ctrl + X` |
| Copy | `Ctrl + C` |
| Paste | `Ctrl + V` |
| Delete | `Delete` |
| Find | `Ctrl + F` |
| Find Next | `F3` |
| Replace | `Ctrl + H` |
| Go To Line | `Ctrl + G` |
| Select All | `Ctrl + A` |
| Insert Time / Date | `F5` |
| Zoom In | `Ctrl + +`, `Ctrl + =`, `Ctrl + Numpad +`, or `Ctrl + Mouse Wheel Up` |
| Reset Zoom | `Ctrl + 0` |
| Zoom Out | `Ctrl + -`, `Ctrl + Numpad -`, or `Ctrl + Mouse Wheel Down` |

---

## Project Structure

```text
.
├── install.sh
├── linotepad.c
├── linotepad.desktop
├── linotepad.svg
├── docs/               # icon preview and size comparison
├── tests/              # GTK regression checks
└── README.md
```

---

## Notes

- Uses only **GTK3**, with no additional widget libraries required.
- Undo/Redo uses a small custom history, limited to 100 groups. GTK user actions and each Replace All are grouped; individual typing events are not coalesced into words. Memory use depends on the size of retained edits.
- Undoing or redoing to the saved revision clears the modified marker.
- File loading accepts UTF-8 without embedded NUL bytes. Unsupported encodings are rejected without changing the current document; existing line endings are preserved.
- The 593-byte SVG icon is installed separately from the executable and scales to different launcher sizes. No PNG icon is needed.
- Font changes made through **Format → Font** stay synchronized with the zoom controls.
- With the pointer over the editor, hold **Ctrl** and scroll up to zoom in or down to zoom out. Smooth scrolling accumulates small deltas into zoom steps; scrolling without Ctrl retains its normal behavior.
- Optimized for a minimal executable size while maintaining native GTK performance.

---

## Verification

Run the GTK regression checks from a graphical session:

```bash
./tests/run.sh
```

These checks use temporary documents and cover grouped undo/redo, saved revisions,
Unicode replacement, invalid input, canceled saves, Delete at EOF, font zoom,
and Ctrl+wheel/smooth scrolling.
They require a working GTK display and the build dependencies above; on a headless
machine, an Xvfb/Xwayland test display can be used. Tests are not installed or
compiled into the application.

See [the size comparison](docs/size-comparison.md) and
[the icon preview](docs/icon-preview.png).

---

## Why Linotepad?

Linotepad was created as a lightweight alternative to heavier graphical text editors, providing a familiar Notepad-like experience while remaining fast, responsive, and extremely small.

---

## License

This project is released under the **MIT License**.
