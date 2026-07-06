# Linotepad

A lightweight Notepad-style text editor for Linux, built with GTK3.

Linotepad is a from-scratch reimplementation of notepad.exe to Linux. Since the original relies on Win32 APIs (RichEdit, HMENU, common dialogs) with no direct Linux equivalent, this version uses GTK3 to provide the same core functionality on Linux.

## Features

- New / Open / Save / Save As
- Print support
- Cut / Copy / Paste / Delete / Select All
- Insert current time/date
- Find, Find Next, Replace, Replace All
- Go to line
- Word wrap toggle
- Font chooser
- Status bar with line/column position
- Zoom in/out with `Ctrl +` / `Ctrl -` or `Ctrl + Scroll Wheel`

## Build

### Dependencies

Linotepad requires GTK3 development headers:

```bash
sudo apt-get install -y libgtk-3-dev
```

> **Note:** On some distros (e.g. Parrot OS / mixed Debian repos), `libgtk-3-dev` may be pinned to a version that conflicts with an already-installed `gir1.2-gtk-3.0`. If `apt-get install` reports unmet dependencies, try:
>
> ```bash
> sudo apt-get install -y -t testing libgtk-3-dev
> ```
>
> or check for a repo/version pin with `apt-cache policy libgtk-3-dev`.

### Compile

```bash
gcc -Os -s -o linotepad linotepad.c $(pkg-config --cflags --libs gtk+-3.0) -Wl,--gc-sections -ffunction-sections -fdata-sections
```

- `-Os` optimizes for binary size
- `-s` strips symbols
- `-ffunction-sections -fdata-sections` + `--gc-sections` lets the linker drop unused code, keeping the binary as small as possible

## Usage

```bash
./linotepad [optional_file.txt]
```

If a file path is given, it's loaded on startup. Otherwise, Linotepad opens with an empty "Untitled" buffer.

## Keyboard Shortcuts

| Action        | Shortcut           |
|---------------|--------------------|
| New           | `Ctrl+N`           |
| Open          | `Ctrl+O`           |
| Save          | `Ctrl+S`           |
| Save As       | `Ctrl+Shift+S`      |
| Print         | `Ctrl+P`           |
| Cut           | `Ctrl+X`           |
| Copy          | `Ctrl+C`           |
| Paste         | `Ctrl+V`           |
| Delete        | `Delete`           |
| Find          | `Ctrl+F`           |
| Find Next     | `F3`               |
| Replace       | `Ctrl+H`           |
| Go To Line    | `Ctrl+G`           |
| Select All    | `Ctrl+A`           |
| Insert Time/Date | `F5`            |
| Zoom In       | `Ctrl +` / `Ctrl+Scroll Up`   |
| Zoom Out      | `Ctrl -` / `Ctrl+Scroll Down` |

## Notes

- Undo is intentionally not implemented — `GtkTextBuffer` has no built-in undo stack; that would require `GtkSourceView` as an additional dependency.
- Font size set via Format → Font stays in sync with the zoom controls.

## License

Licensed under the Apache License, Version 2.0.
