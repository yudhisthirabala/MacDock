# Requirements — macOS Windows Overlay

> Last updated: 2026-04-14 | Status: Baselined (approved via planning session 001)

---

## 1. Functional Requirements

### 1.1 Application Lifecycle

| ID     | Requirement                                                                 | Priority | Status   |
|--------|-----------------------------------------------------------------------------|----------|----------|
| FR-001 | App launches as a single self-contained `.exe` with no installer required  | Must     | Approved |
| FR-002 | On launch, app automatically hides the Windows taskbar                      | Must     | Approved |
| FR-003 | On exit (normal or crash), app restores the Windows taskbar                 | Must     | Approved |
| FR-004 | A system tray icon is present for accessing settings and quitting the app   | Must     | Approved |
| FR-005 | App persists user configuration between sessions                            | Must     | Approved |

### 1.2 Top Menu Bar

| ID     | Requirement                                                                 | Priority | Status   |
|--------|-----------------------------------------------------------------------------|----------|----------|
| FR-010 | A full-width bar is pinned to the top of the screen at all times            | Must     | Approved |
| FR-011 | Bar is always-on-top and does not yield to other windows                    | Must     | Approved |
| FR-012 | Left side shows an Apple logo that opens a simple dropdown menu             | Must     | Approved |
| FR-013 | Center shows the name of the currently active foreground application        | Must     | Approved |
| FR-014 | Right side shows a live clock (updates every second)                        | Must     | Approved |
| FR-015 | Right side shows battery level and charging status                          | Must     | Approved |
| FR-016 | Right side shows current volume level                                       | Must     | Approved |
| FR-017 | Right side shows Wi-Fi connection status                                    | Must     | Approved |
| FR-018 | Global app menu (File, Edit, View...) is deferred to a later version        | Deferred | Approved |

### 1.3 Bottom Dock

| ID     | Requirement                                                                 | Priority | Status   |
|--------|-----------------------------------------------------------------------------|----------|----------|
| FR-020 | Dock is centered horizontally at the bottom of the screen                  | Must     | Approved |
| FR-021 | Dock floats slightly above the screen bottom edge (macOS-style gap)        | Must     | Approved |
| FR-022 | Dock is always-on-top and does not yield to other windows                   | Must     | Approved |
| FR-023 | Dock starts empty on first launch                                           | Must     | Approved |
| FR-024 | User can pin an app by dragging a `.exe` or `.lnk` file onto the Dock      | Must     | Approved |
| FR-025 | App icon is automatically extracted from the `.exe` or `.lnk` file         | Must     | Approved |
| FR-026 | Clicking a pinned app icon launches the app via ShellExecute                | Must     | Approved |
| FR-027 | If the app is already running, clicking its icon brings it to the foreground| Must     | Approved |
| FR-028 | A small dot indicator appears under icons of currently running apps         | Must     | Approved |
| FR-029 | Hover over icons triggers a magnification (fish-eye) animation              | Must     | Approved |
| FR-030 | Icons within ~120px of cursor scale up to ~1.8x; adjacent icons scale down | Must     | Approved |
| FR-031 | Pinned app layout (order, paths) is saved to `pinned_apps.json`            | Must     | Approved |
| FR-032 | User can right-click a Dock icon to unpin or get options                    | Should   | Pending  |
| FR-033 | Dock width expands/contracts as apps are added or removed                   | Should   | Pending  |

### 1.4 Configuration

| ID     | Requirement                                                                 | Priority | Status   |
|--------|-----------------------------------------------------------------------------|----------|----------|
| FR-040 | Pinned apps are stored in `src/config/pinned_apps.json`                    | Must     | Approved |
| FR-041 | Config file is created automatically on first launch if missing             | Must     | Approved |
| FR-042 | Config stores: app name, exe path, icon cache path (optional)              | Must     | Approved |
| FR-043 | Config changes are written immediately on any Dock modification             | Must     | Approved |

---

## 2. Non-Functional Requirements

| ID      | Requirement                                                                | Priority | Status   |
|---------|----------------------------------------------------------------------------|----------|----------|
| NFR-001 | Output must be a single self-contained `.exe` — zero runtime dependencies | Must     | Approved |
| NFR-002 | Dock magnification animation must run at 60fps or better                  | Must     | Approved |
| NFR-003 | App memory footprint should be under 50MB at idle                         | Should   | Approved |
| NFR-004 | App must not interfere with other applications' window management          | Must     | Approved |
| NFR-005 | Taskbar must always be restored on exit, even if the app crashes           | Must     | Approved |
| NFR-006 | Config file must not be corrupted on crash (write-then-replace pattern)    | Must     | Approved |
| NFR-007 | App must support Windows 10 (1903+) and Windows 11                        | Must     | Approved |

---

## 3. Out of Scope (v1)

- Global app menu bar (File, Edit, View... from active app) — deferred to v2
- macOS window decorations (traffic light buttons, title bar replacement)
- Spotlight / universal search overlay
- Multiple monitor support
- Custom themes or color schemes
- Notification center

---

## 4. Requirements Change Log

| Date       | ID     | Change                         | Approved By |
|------------|--------|--------------------------------|-------------|
| 2026-04-14 | ALL    | Initial baseline from planning | Bala        |
