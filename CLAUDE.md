# CLAUDE.md — macOS Windows Overlay Project

> This is the master reference file for Claude Code. Read this file in full at the start of every session before taking any action.

---

## Project Overview

**Name:** macOS Windows Overlay  
**Goal:** A self-contained Windows `.exe` that makes the desktop look and feel like macOS — featuring a top menu bar and an animated bottom Dock.  
**Platform:** Windows only  
**Output:** Single self-contained `.exe`, zero runtime dependencies  
**Status:** In Development — Phase 1

---

## Tech Stack

| Layer              | Technology                                      |
|--------------------|-------------------------------------------------|
| Language           | C++17 (MSVC)                                    |
| UI & Rendering     | Win32 API + Direct2D                            |
| Blur / Compositing | DirectComposition                               |
| Config             | nlohmann/json (header-only, `vendor/nlohmann/`) |
| Build              | CMake 3.20+                                     |

---

## File Reference Map

```
/
├── CLAUDE.md                          ← YOU ARE HERE (master reference)
├── CMakeLists.txt                     ← Build configuration
├── .gitignore
│
├── docs/
│   ├── REQUIREMENTS.md                ← All functional & non-functional requirements
│   ├── DECISIONS.md                   ← All architectural decisions + approval status
│   ├── SESSION_LOG.md                 ← Per-session work log (start/end protocol)
│   ├── CHANGELOG.md                   ← Feature additions and fixes by version
│   ├── LOG.md                         ← Running development notes and blockers
│   └── TESTING.md                     ← Test strategy, test cases, results
│
├── src/
│   ├── main.cpp                       ← Entry point, initializes app, windows, hooks
│   ├── dock/
│   │   ├── DockWindow.h / .cpp        ← Main dock container window
│   │   ├── DockIcon.h / .cpp          ← Individual icon widget (paint, animate, click)
│   │   ├── DockDropTarget.h / .cpp    ← OLE IDropTarget for drag-to-pin (DEC-011)
│   │   └── DockDropValidator.h / .cpp ← Pure-logic file validation + name extraction (testable)
│   ├── menubar/
│   │   ├── MenuBarWindow.h / .cpp     ← Full-width top bar window
│   │   ├── ActiveAppWatcher.h / .cpp  ← Win32 event hook for foreground app name
│   │   └── SystemInfoBar.h / .cpp     ← Clock, battery, volume, Wi-Fi widgets
│   ├── system/
│   │   ├── TaskbarManager.h / .cpp    ← Hide/restore Shell_TrayWnd on launch/exit
│   │   ├── AppBarManager.h / .cpp     ← Reserve screen work area via SHAppBarMessage (DEC-013)
│   │   ├── ProcessMonitor.h / .cpp    ← EnumWindows polling for running indicators
│   │   ├── AppLauncher.h / .cpp       ← ShellExecute to open or focus apps
│   │   ├── SystemInfo.h / .cpp        ← Battery, volume, Wi-Fi via Win32 APIs
│   │   ├── CrashRecovery.h / .cpp     ← Sentinel-file crash recovery for taskbar restore
│   │   └── CompositionHelper.h        ← Header-only: layered-window rendering + primary-monitor size
│   └── config/
│       └── ConfigManager.h / .cpp     ← Read/write %APPDATA%\macOSWin\pinned_apps.json (DEC-028)
│
├── vendor/
│   └── nlohmann/
│       └── json.hpp                   ← Header-only JSON library
│
└── assets/
    └── icons/                         ← Fallback and UI icons (PNG)
```

---

## SDLC Process

### Development Phases

| Phase | Name                  | Description                                                       | Status      |
|-------|-----------------------|-------------------------------------------------------------------|-------------|
| 1     | Skeleton              | Two borderless always-on-top windows appear, taskbar hides       | **Complete** |
| 2     | Dock Core             | Config loading, static icons, click to launch                    | **Complete** |
| 3     | Interaction           | Drag-to-pin, running app indicators                              | **Complete** |
| 4     | Animation             | Dock magnification (fish-eye hover effect)                       | **Complete** |
| 5     | Menu Bar              | System info: clock, battery, volume, Wi-Fi, active app name      | **Complete** |
| 6     | Polish                | Acrylic/blur backgrounds, entrance animations, edge cases        | **In Progress** |

### Working Rules for Claude Code

1. **Always read `CLAUDE.md` first** at session start before touching any file.
2. **Never start a new phase** without the previous phase being verified and working.
3. **Never make architectural decisions unilaterally** — log them in `docs/DECISIONS.md` and wait for user approval before implementing.
4. **Update `docs/LOG.md`** with any blockers, findings, or implementation notes as you work.
5. **Update `docs/CHANGELOG.md`** when a phase or feature is completed.
6. **Never delete or overwrite `pinned_apps.json`** if it contains user data.
7. **Prefer small, focused commits** — one logical change at a time.
8. **All new `.cpp` files must have a corresponding `.h`** — no orphan implementations.

---

## Session Start Protocol

At the beginning of every session, Claude Code MUST:

1. Read `CLAUDE.md` (this file) fully.
2. Read `docs/SESSION_LOG.md` — find the last session entry and review what was done.
3. Read `docs/LOG.md` — check for any open blockers or notes.
4. Read `docs/DECISIONS.md` — check for any pending decisions awaiting user approval.
5. Announce to the user:
   - Current project phase
   - What was completed last session
   - Any pending decisions needing approval
   - Proposed work for this session
6. Wait for user to confirm the plan before writing any code.

**Session start message format:**
```
## Session Start — [DATE]
- Last session: [summary]
- Open blockers: [list or "None"]
- Pending decisions: [list or "None"]
- Proposed work today: [list]
- Ready to proceed? (yes/no)
```

---

## Session End Protocol

At the end of every session, Claude Code MUST:

1. Update `docs/SESSION_LOG.md` with the session summary (see format below).
2. Update `docs/LOG.md` with any new notes or blockers discovered.
3. Update `docs/CHANGELOG.md` if any features were completed.
4. Update phase status in this file if a phase was completed.
5. Update `docs/TESTING.md` — fill in Result and Notes for every test case executed this session; append a row to the phase's Test Run Log with date, tester, build config, and outcome.
6. Tell the user exactly what was done and what to do next session.

**Session end message format:**
```
## Session End — [DATE]
- Completed: [list of things finished]
- Files changed: [list]
- Blockers/notes: [list or "None"]
- Next session should: [list]
```

---

## Decision Approval Process

Any time a decision is needed that affects architecture, file structure, behavior, or user-facing features:

1. Claude Code writes the decision to `docs/DECISIONS.md` with status `PENDING`.
2. Claude Code presents the decision to the user in chat with clear options.
3. User approves or modifies in chat.
4. Claude Code updates the decision status to `APPROVED` or `REJECTED` with the user's choice.
5. Only then does Claude Code implement anything related to that decision.

**Claude Code must never implement a PENDING decision.**

---

## Coding Standards

- **Language standard:** C++17
- **Naming:** PascalCase for classes, camelCase for methods and variables, UPPER_SNAKE for constants
- **Headers:** Use `#pragma once` (not include guards)
- **Win32:** Always check return values; log failures to `docs/LOG.md`
- **Memory:** Prefer RAII; release all COM objects via `Release()` in destructors
- **Comments:** Every class and public method must have a one-line comment explaining purpose
- **No magic numbers:** Define all pixel sizes, timings, and colors as named constants at the top of each file

---

## Key Win32 Reference

| Task                        | API                                              |
|-----------------------------|--------------------------------------------------|
| Hide Windows taskbar        | `FindWindow(L"Shell_TrayWnd")` + `ShowWindow`    |
| Detect active app           | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`       |
| List running apps           | `EnumWindows` + `GetWindowThreadProcessId`       |
| Extract app icon            | `ExtractAssociatedIcon` or `SHGetFileInfo`       |
| Launch app                  | `ShellExecute`                                   |
| Focus running app           | `SetForegroundWindow`                            |
| Battery info                | `GetSystemPowerStatus`                           |
| Volume info                 | `IAudioEndpointVolume` (COM)                     |
| Wi-Fi info                  | `WlanApi` — `WlanQueryInterface`                 |
| Borderless window           | `WS_POPUP` style, no `WS_CAPTION`               |
| Always on top               | `SetWindowPos` with `HWND_TOPMOST`               |
| Accept drag-drop            | `DragAcceptFiles` + `WM_DROPFILES`               |

---

*Last updated: 2026-05-07 | Session 015 — Config moved to %APPDATA% (DEC-028), run-at-startup toggle (DEC-029), crash recovery sentinel, dead code cleanup. DComp acrylic deferred.*
