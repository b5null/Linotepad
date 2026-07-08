# Linotepad

A lightweight **Notepad-style text editor for Linux**, built with GTK3.

Linotepad is a from-scratch reimplementation inspired by Microsoft's classic **Notepad**. Since the original application depends on Win32 APIs (RichEdit, HMENU, Common Dialogs, etc.), this project recreates the same look and core functionality using native GTK3 widgets on Linux.

The resulting optimized binary is approximately **32 KB**, making it one of the smallest fully-featured GUI text editors available for Linux.

---

## Features

- New Window
- Open
- Save
- Save As
- Print support
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
- Status bar with Line / Column position
- Zoom using keyboard shortcuts or mouse wheel
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

An installation script is included with the project.

It will:

- Compile Linotepad
- Optimize the executable
- Install the binary into `/usr/bin`
- Install the application icon into `~/.local/share/icons`
- Install the desktop launcher into `~/.local/share/applications`
- Refresh the desktop application database

Simply run:

```bash
chmod +x install.sh
./install.sh
```

---

## Running

```bash
linotepad
```

or open an existing file:

```bash
linotepad myfile.txt
```

---

## Keyboard Shortcuts

| Action | Shortcut |
|---------|----------|
| New Window | `Ctrl + N` |
| Open | `Ctrl + O` |
| Save | `Ctrl + S` |
| Save As | `Ctrl + Shift + S` |
| Print | `Ctrl + P` |
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
| Zoom Out | `Ctrl + -`, `Ctrl + Numpad -`, or `Ctrl + Mouse Wheel Down` |

---

## Project Structure

```text
.
├── install.sh
├── linotepad.c
├── linotepad.desktop
├── linotepad.png
├── LICENSE
└── README.md
```

---

## Notes

- Uses only **GTK3**, with no additional widget libraries required.
- Undo/Redo is intentionally not implemented because `GtkTextBuffer` does not provide an undo stack. Supporting it would require an additional dependency such as GtkSourceView.
- Font changes made through **Format → Font** stay synchronized with the zoom controls.
- Optimized for a minimal executable size while maintaining native GTK performance.

---

## Why Linotepad?

Linotepad was created as a lightweight alternative to heavier graphical text editors, providing a familiar Notepad-like experience while remaining fast, responsive, and extremely small.

---

## License

This project is released under the **MIT License**.
