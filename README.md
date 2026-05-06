# macOS Windows Overlay

A self-contained Windows `.exe` that gives Windows 11 a macOS-style desktop:
a top menu bar with system info and a bottom Dock with magnifying icons.

Built in C++17 with the Win32 API, Direct2D, and GDI+. No runtime dependencies.

## Features

- **Top menu bar**: clock, battery, volume, Wi-Fi, active app name, media controls
- **Bottom Dock**:
  - Pinned apps with high-quality bicubic-scaled icons
  - macOS-style fish-eye magnification on hover
  - Running-app indicator dots
  - Drag-and-drop `.exe` / `.lnk` from File Explorer to pin
  - Click to launch or focus
- **Work-area reservation** so maximized windows respect the bars
- **System tray icon** for quitting (Ctrl+Alt+Q also quits)
- **Auto-hides the Windows taskbar** while running, restores on exit

## Tech stack

| Layer            | Technology                            |
|------------------|---------------------------------------|
| Language         | C++17 (MSVC)                          |
| UI               | Win32 API + GDI+                      |
| Window surface   | `WS_EX_LAYERED` + `UpdateLayeredWindow` (premultiplied DIB) |
| Config           | `nlohmann/json` (header-only)         |
| Build            | CMake 3.20+                           |

## Build

Requires Visual Studio 2022+ with the C++ workload and CMake.

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
build\bin\Release\macOSWin.exe
```

## Status

Phases 1–5 complete. Phase 6 (Polish) in progress — DEC-027's
DirectComposition rewrite was attempted for real Win11 acrylic but reverted
due to GDI/D2D alpha incompatibilities; the stable layered-window path is
shipping instead.

## Project layout

```
src/
├── main.cpp              — entry point, OLE/GDI+ init, message loop
├── dock/                 — Dock window, icons, drag-drop
├── menubar/              — top bar, system widgets, active-app watcher
├── system/               — taskbar hide, app-bar work area, process monitor,
│                           launcher, system info, rendering helper
└── config/               — pinned_apps.json + ConfigManager

docs/                     — REQUIREMENTS, DECISIONS, SESSION_LOG, CHANGELOG
vendor/nlohmann/          — bundled JSON header
```

## License

MIT.
