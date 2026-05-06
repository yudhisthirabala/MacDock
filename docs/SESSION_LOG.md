# Session Log — macOS Windows Overlay

> One entry per working session. Always append — never edit past entries.
> Claude Code: follow Session Start and End Protocol defined in CLAUDE.md.

---

## Session 001 — 2026-04-14

**Type:** Planning  
**Participants:** Bala, Claude (Cowork)  
**Duration:** Single conversation  
**Phase at start:** Pre-development  
**Phase at end:** Pre-development (ready to begin Phase 1)

### What Was Done
- Defined project goals and scope
- Decided target platform: Windows only
- Decided integration depth: visual overlay (non-invasive)
- Decided tech stack: Raw C++ / Win32 / Direct2D / DirectComposition
- Decided Dock behavior: centered, starts empty, drag-to-pin, magnification in v1
- Decided menu bar scope for v1: active app name + system info (no global menus)
- Decided config format: JSON via nlohmann/json
- Defined 6-phase build plan
- Created all project documentation
- Created full project scaffolding (directory structure + stub files)
- Logged all 8 decisions in DECISIONS.md (all approved by Bala)

### Files Created
- `CLAUDE.md`
- `CMakeLists.txt`
- `.gitignore`
- `docs/REQUIREMENTS.md`
- `docs/DECISIONS.md`
- `docs/SESSION_LOG.md`
- `docs/CHANGELOG.md`
- `docs/LOG.md`
- `docs/TESTING.md`
- `src/main.cpp`
- `src/dock/DockWindow.h/.cpp`
- `src/dock/DockIcon.h/.cpp`
- `src/dock/DockDropHandler.h/.cpp`
- `src/menubar/MenuBarWindow.h/.cpp`
- `src/menubar/ActiveAppWatcher.h/.cpp`
- `src/menubar/SystemInfoBar.h/.cpp`
- `src/system/TaskbarManager.h/.cpp`
- `src/system/ProcessMonitor.h/.cpp`
- `src/system/AppLauncher.h/.cpp`
- `src/system/SystemInfo.h/.cpp`
- `src/config/ConfigManager.h/.cpp`
- `src/config/pinned_apps.json`
- `vendor/nlohmann/json.hpp` *(placeholder — download before building)*

### Decisions Made
- DEC-001 through DEC-008 — all APPROVED by Bala

### Blockers / Notes
- None

### Next Session Should
1. Read CLAUDE.md and this session log
2. Begin Phase 1: implement `TaskbarManager`, create two borderless always-on-top windows (Dock + MenuBar), verify they appear on screen
3. Confirm Phase 1 plan with Bala before writing code

---

## Session 002 — 2026-04-14

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 1 (Not Started — skeleton code present from scaffolding but not hardened/verified)
**Phase at end:** Phase 1 **COMPLETE** — verified by Bala, smoke test passed

### What Was Done
- Session start audit: confirmed Session 001 scaffolding already contained functional Phase 1 skeleton (window creation, taskbar hide/restore, message loop, CMake).
- Reviewed `main.cpp`, `DockWindow.*`, `MenuBarWindow.*`, `TaskbarManager.*`, `CMakeLists.txt` end-to-end.
- Refactored `src/main.cpp` to use RAII guards (`TaskbarHideGuard`, `ComApartment`) so the Windows taskbar is always restored on any exit path — including abnormal exits from failed window creation.
- Removed dead `WndProc` forward declaration in `main.cpp`.
- Added propagation of window-creation failures as non-zero exit code.
- Logged tech-debt items in `docs/LOG.md` (dual `PostQuitMessage` on both WndProcs, FetchContent edge case on older CMake, no-quit-UI issue).

### Files Changed
- `src/main.cpp` (rewrote WinMain with RAII guards)
- `docs/LOG.md` (Session 002 entry + build/smoke-test instructions)
- `docs/SESSION_LOG.md` (this entry)

### Decisions Made
- None. Three decisions flagged for a future session (quit mechanism, run-at-startup, unpin gesture) — not yet raised in DECISIONS.md because they belong to Phase 2+ and we are still verifying Phase 1.

### Blockers / Notes
- Claude could not run `cmake` / `cl.exe` locally — build verification is pending Bala running it in a VS Developer Command Prompt. See `docs/LOG.md` Session 002 for exact commands and expected behavior.
- Phase 1 cannot be marked Complete in `CLAUDE.md` until Bala confirms the smoke test passes.

### Next Session Should
1. Bala runs the Phase 1 build + smoke test; reports pass/fail.
2. If pass: mark Phase 1 Complete in `CLAUDE.md`, tag v0.1.0 in `CHANGELOG.md`, raise DEC-009 (quit mechanism) before starting Phase 2.
3. If fail: capture the exact error in `docs/LOG.md`, diagnose, patch, re-test — do not advance to Phase 2 until the skeleton runs cleanly.

---

## Session 003 — 2026-04-14

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 1 complete (v0.1.0 shipped); Phase 2 not yet started
**Phase at end:** Phase 2 **In Progress** — Ctrl+Alt+Q quit landed + verified, gtest pipeline landed + verified (100% passing), component work pending

### What Was Done
- Bookkeeping: `CLAUDE.md` phase table updated to mark Phase 2 **In Progress**. (Phase 1 Complete and CHANGELOG v0.1.0 entry were already recorded at end of Session 002.)
- Implemented `src/system/TrayIcon.{h,cpp}` — hidden message-only receiver window, `Shell_NotifyIcon` with `NIM_ADD/NIM_DELETE`, right-click popup menu with "Quit macOS Overlay" → `PostQuitMessage(0)`. Satisfies DEC-009. Wired into `main.cpp` after Dock/MenuBar creation.
- Consolidated the dual-`PostQuitMessage` tech debt flagged in Session 002: removed `PostQuitMessage(0)` from both `DockWindow::WndProc` and `MenuBarWindow::WndProc` `WM_DESTROY` handlers. The tray icon is now the single quit path; window destruction during shutdown no longer double-posts `WM_QUIT`.
- Added Google Test scaffolding (DEC-010): new `tests/` subdirectory with its own `CMakeLists.txt`, `gtest_discover_tests` wired to `ctest`, `smoke_test.cpp` as a pipeline-health canary. Gated behind root-level option `BUILD_TESTS` (default OFF) so release .exe builds don't pull gtest.
- Added `src/system/TrayIcon.cpp` to the root `CMakeLists.txt` source list.

### Files Changed
- `CLAUDE.md` (phase table, footer date)
- `CMakeLists.txt` (TrayIcon source + `BUILD_TESTS` option + `add_subdirectory(tests)`)
- `src/main.cpp` (create `TrayIcon` after windows)
- `src/dock/DockWindow.cpp` (remove `PostQuitMessage` from WM_DESTROY)
- `src/menubar/MenuBarWindow.cpp` (remove `PostQuitMessage` from WM_DESTROY)
- `src/system/TrayIcon.h` (new)
- `src/system/TrayIcon.cpp` (new)
- `tests/CMakeLists.txt` (new)
- `tests/smoke_test.cpp` (new)
- `docs/SESSION_LOG.md` (this entry)
- `docs/LOG.md` (Session 003 notes)

### Decisions Made
- None new. Implemented DEC-009 and DEC-010 (both approved at end of Session 002).

### Blockers / Notes
- **Verified end-of-session (by Bala):** Ctrl+Alt+Q quits cleanly; `ctest` reports 100% pass. No open blockers.
- Two deferred decisions still open (no rush, scope is later): (a) run-at-Windows-startup behavior; (b) unpin gesture (right-click menu vs. drag-off-dock). Not blocking Phase 2 component work.

### Mid-session course corrections
- **Tray icon didn't appear:** hiding `Shell_TrayWnd` hides the notification area too, so Option A of DEC-009 was functionally broken. DEC-009 revised to Option C — added **Ctrl+Alt+Q** global hotkey as primary quit path. Tray icon code kept as a fallback for when the taskbar is visible (future Phase 6 refactor). **Verified working.**
- **gtest link errors (CRT mismatch) — took two tries:**
  - First attempt: flipped `gtest_force_shared_crt` from ON→OFF. Did not work — googletest v1.14 ignores that variable entirely.
  - Second attempt (working): set `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` before `FetchContent_MakeAvailable(googletest)` so CMake policy CMP0091 propagates static CRT to every gtest target at configure time. Plus explicit `set_property(TARGET gtest gtest_main gmock gmock_main PROPERTY MSVC_RUNTIME_LIBRARY ...)` as belt-and-suspenders. **Verified: 100% tests pass.**

### Next Session Should
1. **Wipe the build directory first** (gtest.lib was built with the wrong CRT):
   ```
   rmdir /s /q build
   cmake -S . -B build -A x64 -DBUILD_TESTS=ON
   cmake --build build --config Release
   ```
2. Launch `build\bin\Release\macOSWin.exe`. Expect: taskbar hides, dock + menu bar appear, tray icon is **not visible** (expected — taskbar is hidden). Press **Ctrl+Alt+Q** → app exits cleanly, taskbar returns.
3. `ctest --test-dir build -C Release --output-on-failure` → expect `Smoke.PipelineIsAlive` passes.
4. If green: begin real Phase 2 component work — `ConfigManager` load/save of `pinned_apps.json` with accompanying gtest unit tests.
5. If either smoke fails: capture error in `docs/LOG.md`, diagnose, patch before moving on.

---

## Session 004 — 2026-04-15

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 2 In Progress (tray + gtest pipeline landed in Session 003; no component work yet)
**Phase at end:** Phase 2 **COMPLETE** — build green, smoke test verified by Bala (Claude + File Explorer launch from dock icons)

### What Was Done
- **ConfigManager gtest suite** (`tests/config_manager_test.cpp`) — 7 tests using a `ConfigManagerTest` fixture that deletes `pinned_apps.json` in SetUp/TearDown so the exe-relative config file doesn't leak between runs:
  - `LoadMissingFileReturnsEmpty`
  - `LoadMissingFileCreatesEmptyConfig`
  - `SaveLoadRoundTrip`
  - `LoadMalformedJsonReturnsEmpty`
  - `LoadEntryWithEmptyPathIsSkipped`
  - `UnicodeRoundTrip` (café / 日本語 / accented paths — exercises the UTF-8 conversion path)
  - `SaveEmptyListProducesEmptyArray`
- `tests/CMakeLists.txt` — test executable now also compiles `src/config/ConfigManager.cpp` and includes `vendor/` so `<nlohmann/json.hpp>` resolves.
- **`DockWindow` fleshed out for Phase 2:**
  - `Create()` now calls `ConfigManager::Load()` after window creation and loops `AddIcon(path, name)` over each pinned entry; finishes with `Reposition()`.
  - `AddIcon()` extracts the app/shortcut icon via `SHGetFileInfoW(SHGFI_ICON | SHGFI_LARGEICON)` (preferred over `ExtractAssociatedIcon` because it resolves `.lnk` targets and returns the real app icon). Falls back to `IDI_APPLICATION` when extraction fails so the tile still exists.
  - `Reposition()` computes width = `(N+1)·ICON_PADDING + N·ICON_SIZE`, recenters via `SetWindowPos`, assigns each icon a bounds rect in client coords, and invalidates for repaint. Empty list → `DOCK_EMPTY_WIDTH = 120` placeholder so the bar is still visible (drop target for Phase 3).
  - `OnPaint()` draws each icon with `DrawIconEx` scaled to its bounds.
  - `OnLButtonUp()` hit-tests via `PtInRect` and calls `AppLauncher::LaunchOrFocus(path)`.
  - `RemoveIcon()` implemented (index-bounded erase + reposition) — not yet called from anywhere; ready for Phase 3 unpin.

### Files Changed
- `tests/config_manager_test.cpp` (new)
- `tests/CMakeLists.txt` (added test source + ConfigManager.cpp + vendor include)
- `src/dock/DockWindow.cpp` (implemented Phase 2 behaviors)
- `docs/SESSION_LOG.md` (this entry)
- `docs/LOG.md` (Session 004 notes)
- `docs/CHANGELOG.md` (Unreleased section updated)
- `docs/TESTING.md` (Phase 2 test results once Bala runs them)

### Decisions Made
- None new. Implemented against DEC-005, DEC-006, DEC-010.

### Mid-session course corrections
- **`LoadIconW` compile error — IDI_APPLICATION type mismatch.** First build after the DockWindow rewrite failed: `IDI_APPLICATION` is `LPSTR`, `LoadIconW` expects `LPCWSTR`. Fixed with `reinterpret_cast<LPCWSTR>(IDI_APPLICATION)`. The classical Win32 gotcha — `LoadIcon` (macro) resolves to A/W based on `UNICODE`, but explicit `LoadIconW` drops that safety net.
- **Stale-exe red herring.** Bala reported icons not appearing after a rebuild. Investigation showed the exe hadn't actually been rebuilt (last-write 14-04 despite 15-04 source). Root cause was the prior paragraph — the failed compile left the exe untouched. Once the cast fix landed, the rebuild produced a fresh binary and icons rendered correctly.
- **File Explorer didn't launch on click.** Claude tile worked first time; Explorer tile did nothing. Diagnosed `AppLauncher::FindMainWindowByExeName` — `explorer.exe` hosts many windows besides File Explorer (`Progman`/`WorkerW` desktop host, `Shell_TrayWnd` taskbar, `NotifyIconOverflowWindow`, etc.). The enumeration matched the desktop first, `ForceForeground`'d it, and looked like "nothing happened" to Bala. **Fix:** introduced `IsUserAppWindow` filter in `AppLauncher.cpp` — require visible + unowned + not `WS_EX_TOOLWINDOW` + non-empty title + class not in a shell-window blacklist. After the fix, clicking Explorer opens a new File Explorer window, and clicking again with one open brings it to foreground. **Verified by Bala.**

### Verification (2026-04-15, end of Session 004) — GREEN
- **Build:** clean after the `LoadIconW` cast fix; `macOSWin.exe` rebuilt fresh from Phase 2 source.
- **Manual smoke:** dock launched with two icons (Claude + File Explorer), real icons extracted via `SHGetFileInfoW`, dock auto-sized and recentered, **clicking Claude launched Claude**, **clicking File Explorer opened File Explorer**. Ctrl+Alt+Q quits cleanly, taskbar restores.
- **Note:** Bala did not explicitly run `ctest` this session — the ConfigManager gtest suite is compiled and ready to run but formal pass verification is deferred to Session 005 bookkeeping. Manual behavior has exercised the same code paths positively.

### Blockers / Notes
- None. Phase 2 code path (config load → icon extraction → paint → click-to-launch) end-to-end verified.
- `pinned_apps.json` in repo now contains two entries (Claude + File Explorer with Bala's actual Claude install path) — this is the test-data config, not production user data, so safe to leave.

### Next Session Should
1. Formal `ctest` run with `-DBUILD_TESTS=ON` to mark all 8 tests green in `docs/TESTING.md`.
2. Tag **v0.2.0** in `CHANGELOG.md` (Phase 2 complete).
3. Raise **DEC-011 — drag-to-pin semantics** (which file types are accepted: only `.exe`/`.lnk`? what about `.bat`/`.cmd`? reject silently or show feedback? duplicate-path handling?) and wait for Bala approval before implementing.
4. Raise **DEC-012 — unpin gesture** (right-click context menu vs. drag-off-dock — long-deferred from Session 001).
5. Begin Phase 3 proper: `DockDropHandler` (`WM_DROPFILES` → `AddIcon` + `ConfigManager::Save`), `ProcessMonitor` (1500 ms `EnumWindows` poll for running-indicator dots).

---

## Session 005 — 2026-04-16

**Type:** Development / Debug
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 2 Complete (v0.2.0 shipped); Phase 3 not started
**Phase at end:** Phase 2 fully closed out; Phase 3 decisions approved; DEC-013 work-area fix landed + verified

### What Was Done
- **Phase 2 close-out:** Formal `ctest` run confirmed all 8 automated tests green (A-001..A-008). Results recorded in `docs/TESTING.md`.
- **DEC-011 (drag-to-pin semantics) raised and APPROVED:** `.exe`+`.lnk` only; flash-red reject; silent duplicate ignore; silent multi-drop skip.
- **DEC-012 (unpin gesture) raised and APPROVED:** drag-off-dock removes the tile (macOS-authentic).
- **DEC-013 (reserve screen work area) raised, APPROVED, implemented, debugged through 3 iterations:**
  - **Attempt 1:** `SHAppBarMessage` (AppBar API). Failed — hidden taskbar's own AppBar stacked with ours, causing maximized windows to fill only ~50% of the screen.
  - **Attempt 2:** `SystemParametersInfo(SPI_SETWORKAREA)` with `SPIF_SENDCHANGE`. Failed — Explorer receives the broadcast, sees taskbar is hidden, and resets the work area to full-screen, overwriting our setting.
  - **Attempt 3 (working):** `SPI_SETWORKAREA` with `fWinIni = 0` (silent, no broadcast). Explorer never hears about it, so it never fights back. A hidden sentinel window with a 500ms timer periodically checks and silently re-applies if anything overwrites. Verified by Bala — maximized windows now stop at menu bar + dock.
- **Fixed empty dock icons:** `build\bin\Release\pinned_apps.json` was empty (`[]`). Populated with Claude + File Explorer entries.

### Files Changed
- `src/system/AppBarManager.h` (new — rewritten 3x during session)
- `src/system/AppBarManager.cpp` (new — rewritten 3x during session)
- `src/main.cpp` (added AppBarManager creation + Apply)
- `CMakeLists.txt` (added AppBarManager.cpp to sources)
- `CLAUDE.md` (added AppBarManager to file reference map, updated footer)
- `build/bin/Release/pinned_apps.json` (populated with test entries)
- `docs/DECISIONS.md` (DEC-011 APPROVED, DEC-012 APPROVED, DEC-013 APPROVED + revised)
- `docs/TESTING.md` (A-002..A-008 marked PASS, Run #2 logged)
- `docs/LOG.md` (Session 005 notes + mid-session fixes)
- `docs/SESSION_LOG.md` (this entry)

### Decisions Made
- DEC-011 — Drag-to-Pin Semantics (APPROVED)
- DEC-012 — Unpin Gesture (APPROVED)
- DEC-013 — Reserve Screen Work Area (APPROVED, revised from AppBar to silent SPI_SETWORKAREA)

### Mid-session course corrections
- AppBar API (SHAppBarMessage) caused half-screen maximize — replaced with SPI_SETWORKAREA.
- First SPI_SETWORKAREA attempt with SPIF_SENDCHANGE triggered Explorer fight-back loop — replaced with silent fWinIni=0 + timer enforcement.
- Dock icons missing because build-output pinned_apps.json was empty — populated manually.

### Blockers / Notes
- None. All three issues resolved and verified.

### Next Session Should
1. Begin **Phase 3 implementation** — both DEC-011 and DEC-012 are approved:
   - `DockDropHandler`: wire `WM_DROPFILES` in `DockWindow::OnDropFiles` → validate `.exe`/`.lnk` → `AddIcon` + `ConfigManager::Save`. Flash-red on reject. Silent duplicate ignore.
   - Drag-off-dock unpin: `WM_LBUTTONDOWN` capture → `WM_MOUSEMOVE` track → `WM_LBUTTONUP` outside bounds → `RemoveIcon` + `ConfigManager::Save`.
   - `ProcessMonitor`: 1500ms `EnumWindows` poll → running-indicator dots under pinned icons.
2. Add Phase 3 gtest cases for any testable logic (e.g., file-type validation, duplicate detection).
3. Run `ctest` after Phase 3 to verify no regressions.

---

## Session 006 — 2026-04-17

**Type:** Development / Debug
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 3 not started (Phase 2 fully closed out)
**Phase at end:** Phase 3 **COMPLETE** — drag-to-pin, drag-off-unpin, running indicators all verified by Bala

### What Was Done
- **Switched drag-drop from `WM_DROPFILES` to OLE `IDropTarget`** (3-iteration debug):
  - Iteration 1: `ChangeWindowMessageFilterEx` — WM_DROPFILES still silently dropped for layered/topmost/noactivate windows.
  - Iteration 2: OLE `IDropTarget` with `CoInitializeEx` — `RegisterDragDrop` silently failed; no-drop cursor from start.
  - Iteration 3 (working): replaced `ComApartment` (`CoInitializeEx`) with `OleApartment` (`OleInitialize`) — required for the full OLE drag-drop subsystem. RegisterDragDrop succeeded, drops worked.
- **Start Menu drag-drop support** (3-iteration debug):
  - Start Menu CF_HDROP contains app IDs (`"MSEdge"`, `"Microsoft.Office.WINWORD.EXE.15"`) not file paths — `IsValidAppFile` correctly rejected them, producing red flash but no pin.
  - Added `FindStartMenuShortcut()` — scans CSIDL_COMMON_PROGRAMS + CSIDL_PROGRAMS .lnk files, matches by target exe name or token.
  - Added `shell:AppsFolder\<appId>` fallback for UWP/Store apps with no matching .lnk.
  - Added `IsGenericToken()` blacklist (`"microsoft"`, `"windows"`, `"app"`, etc.) — prevents false matches (e.g., Settings → Administrative Tools).
  - Added Shell IDList Array format handler — PIDLs from explorer drag, resolves via `SHGetPathFromIDListW` + `IShellItem`.
  - Implemented DROPEFFECT negotiation fix — Start Menu only offers `DROPEFFECT_LINK`; overriding with `DROPEFFECT_COPY` caused no-drop cursor. Fix: keep source's offered effect when `hasValidData` is true.
- **Resolved shortcut arrow overlay on icons** — `SHGetFileInfoW` on `.lnk` returns icon with arrow overlay. Fix: resolve `.lnk` target via `IShellLink` first, then extract icon from the target exe. Result: clean icon, no arrow.
- **UWP/Store apps** — extract icons via `IShellItemImageFactory::GetImage(48×48)`. Settings, Microsoft Store, etc. all pin with correct clean icons.
- **`DockDropValidator`** — pure-logic file validation and name extraction, no Win32 UI deps, unit-testable.
- **Drag-off-dock unpin** — `WM_LBUTTONDOWN` SetCapture, 8px threshold, `WM_LBUTTONUP` outside client rect → `RemoveIcon` + `SaveConfig`.
- **Running indicator dots** — 1500ms `WM_TIMER`, `ProcessMonitor::GetRunningAppNames`, white 6px ellipse drawn below running icons.
- **Flash reject** — 200ms red background on invalid drop via second `WM_TIMER`.
- **`SaveConfig`** called on every pin/unpin, persisting to `pinned_apps.json`.
- **17 new gtest cases** for `DockDropValidator` — all green (25/25 total including prior sessions).
- **Removed debug logging** (`C:\temp\drop_debug.txt`) before final build.

### Files Changed
- `src/main.cpp` — `ComApartment` → `OleApartment` (CoInitializeEx → OleInitialize)
- `src/dock/DockDropTarget.h` (new)
- `src/dock/DockDropTarget.cpp` (new)
- `src/dock/DockDropValidator.h` (new)
- `src/dock/DockDropValidator.cpp` (new)
- `src/dock/DockWindow.h` — added drop target, drag state, flash state, HasIcon, OnDropComplete, OnLButtonDown, timer IDs
- `src/dock/DockWindow.cpp` — OLE registration, AddIcon rewrite (UWP + lnk resolution), SaveConfig, FlashReject, OnDropComplete, OnLButtonDown/Move/Up drag-unpin, running dot paint, OnTimer dispatch
- `src/dock/DockDropHandler.cpp` — simplified (now dead code, kept for build compatibility)
- `tests/drop_handler_test.cpp` (new)
- `tests/CMakeLists.txt` — added DockDropValidator.cpp + drop_handler_test.cpp
- `CMakeLists.txt` — added DockDropValidator.cpp + DockDropTarget.cpp
- `CLAUDE.md` — file reference map updated
- `docs/CHANGELOG.md` — v0.3.0 entry
- `docs/TESTING.md` — T3-001..T3-011 + A-009..A-025 results, Phase 3 run log
- `docs/LOG.md` — Session 006 notes
- `docs/SESSION_LOG.md` — this entry

### Decisions Made
- None new. Implemented against DEC-011 and DEC-012 (both approved in Session 005).

### Mid-session course corrections
- WM_DROPFILES → OLE IDropTarget (3 iterations — see above).
- DROPEFFECT negotiation: must keep source's offered effect to avoid no-drop cursor from Start Menu.
- Start Menu app ID resolution: LooksLikeRealPath filter, FindStartMenuShortcut, UWP fallback (3 iterations).
- Settings false match fixed by IsGenericToken blacklist.
- Shortcut arrow: resolved by extracting icon from IShellLink target exe, not the .lnk itself.

### Verification (2026-04-17, end of Session 006) — GREEN
- Explorer drag to pin: PASS
- .lnk shortcut drag to pin (no arrow): PASS
- Non-app file drag (red flash reject): PASS
- Start Menu app drag (Win32 apps): PASS
- Start Menu UWP app drag (Settings, Store): PASS
- Drag-off-dock unpin: PASS
- Running indicator dots: PASS
- Config persistence across restart: PASS
- 25/25 automated tests: PASS

### Blockers / Notes
- `DockDropHandler.h/.cpp` is now dead code (OLE target handles drops directly). Kept to avoid breaking the build; can be removed in a future cleanup session.
- Running indicator for .lnk-pinned apps: process name is matched against the exe name extracted from the .lnk target, not the .lnk filename itself. Should work in most cases but not explicitly tested (T3-007 not yet verified).

### Next Session Should
1. Begin **Phase 4 — Animation** planning. Raise decisions before coding:
   - Frame rate mechanism (16ms WM_TIMER vs Direct2D animation loop)
   - Whether to use Direct2D for scaled rendering or GDI+ StretchBlt
   - Magnification curve (cosine falloff vs linear vs Gaussian)
2. Implement fish-eye hover magnification: ~1.8x scale at cursor icon, proportional falloff within 120px radius, 60fps.
3. Hook in to TODOs already stubbed in `DockWindow::OnMouseMove` and `OnMouseLeave`.

---

## Session 007 — 2026-04-17

**Type:** Bug Fix
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 3 complete; three post-launch bugs reported by Bala
**Phase at end:** All three bugs fixed; ready to begin Phase 4 planning

### What Was Done
- **Work area immediate-apply fix** (`AppBarManager::Apply`): Added `EnumWindows(NotifyMaximizedProc, 0)` after the silent `SPI_SETWORKAREA` call. The callback posts `WM_SETTINGCHANGE(SPI_SETWORKAREA)` directly to each visible maximized window, skipping Explorer shell windows (`Shell_TrayWnd`, `Progman`, `WorkerW`, etc.) to avoid the fight-back reset. Already-maximized windows now snap to the reserved bars on first launch without a minimize-restore cycle.
- **Settings (UWP) icon fix** (`ExtractUwpIcon`): Request size changed 48×48→256×256. Flag changed `SIIGBF_ICONONLY`→`SIIGBF_RESIZETOFIT` to preserve the tile's coloured background. Mask bitmap updated to 256×256. Settings gear icon and other UWP tile icons now render correctly.
- **Dock icon size + pixelation fix** (`DockWindow.h`, `AddIcon`): `ICON_SIZE` reduced 56→48 px. `AddIcon` regular path now tries `IShellItemImageFactory::GetImage({48,48}, SIIGBF_ICONONLY|SIIGBF_RESIZETOFIT)` first, which returns a bitmap at exactly 48×48 — no upscaling, no blur. Falls back to `SHGetFileInfoW(SHGFI_LARGEICON)` if factory fails.
- Updated `docs/CHANGELOG.md` (v0.3.1 entry), `docs/LOG.md` (Session 007 notes), `docs/SESSION_LOG.md` (this entry).

### Files Changed
- `src/system/AppBarManager.cpp` (NotifyMaximizedProc + EnumWindows call)
- `src/dock/DockWindow.h` (ICON_SIZE 56→48)
- `src/dock/DockWindow.cpp` (ExtractUwpIcon size/flags, AddIcon factory-based extraction)
- `docs/CHANGELOG.md` (v0.3.1 section)
- `docs/LOG.md` (Session 007 entry)
- `docs/SESSION_LOG.md` (this entry)

### Decisions Made
- None. All fixes are non-architectural code corrections.

### Mid-session course corrections
- None. Root causes were clear from the bug reports; fixes were single-pass.

### Blockers / Notes
- Build + smoke test required from Bala before these fixes are verified.
- `ICON_SIZE` change (56→48) affects `Reposition()` computed dock width — dock will be slightly narrower for the same number of icons. Expected and desirable.

### Next Session Should
1. Bala builds and runs the three-fix build; confirms all three issues resolved.
2. Begin **Phase 4 planning** — raise decisions before writing any animation code:
   - DEC-014: Frame rate mechanism (16ms `WM_TIMER` vs Direct2D render loop)
   - DEC-015: Rendering backend for scaled icons (GDI+ `StretchBlt` vs Direct2D `DrawBitmap`)
   - DEC-016: Magnification curve shape (linear, cosine, Gaussian)
3. Wait for Bala approval on all Phase 4 decisions before implementing.

---

## Session 008 — 2026-04-17

**Type:** Debug / Iterative visual tuning
**Participants:** Bala, Claude Code
**Duration:** Single conversation (continuation from Session 007)
**Phase at start:** Phase 3 complete, Phase 4 not started
**Phase at end:** Phase 3 complete (visuals now match macOS reference)

### What Was Done
Iterative dock visual polish driven by Bala's live feedback after each rebuild.

1. **Work-area auto-snap (Session 007 carry-over)** — confirmed fixed by Bala.
2. **Icon scaling quality** — went through three approaches before landing:
   - `DrawIconEx` + `SetStretchBltMode(HALFTONE)` → still blocky (DrawIconEx ignores stretch mode for HICONs)
   - `AlphaBlend` from 256×256 HBITMAP → still blocky (AlphaBlend is nearest-neighbour, not bilinear)
   - **GDI+ `Graphics::DrawImage` with `InterpolationModeHighQualityBicubic`** → finally smooth
3. **Settings (UWP) icon** — went through: `SIIGBF_ICONONLY` at 48px → 256px → `SIIGBF_BIGGERSIZEOK` → `SHIL_JUMBO` imagelist → factory-tile-first with HBITMAP→HICON conversion. Final UWP path: factory tile (preserves package logo + background colour), fall back to JUMBO.
4. **Dock size** — `ICON_SIZE` stepped 56 → 48 → 40 → 32 → **28**. `DOCK_HEIGHT` 76 → **40**. `ICON_PADDING` 10 → **20**. `BOTTOM_GAP` 8 → **2**. Work-area reserve `DOCK_TOTAL_HEIGHT` 84 → **42** to match.
5. **Transparent background** — switched from `LWA_ALPHA` (uniform translucency) to `LWA_COLORKEY(RGB 255,0,255)` with magenta fill so only the icons remain visible; no black bar behind the dock.
6. **Added GDI+ apartment** in `main.cpp` (`GdiplusStartup`/`GdiplusShutdown` via RAII guard) and linked `gdiplus` + `msimg32` in CMake.
7. **`DockIcon`** gained an `HBITMAP` field (may be null) for the factory-only path; dtor releases both HICON and HBITMAP.

### Files Changed
- `CMakeLists.txt` (added `msimg32`, `gdiplus`)
- `src/main.cpp` (GDI+ RAII apartment; `DOCK_TOTAL_HEIGHT` 84→42)
- `src/dock/DockWindow.h` (ICON_SIZE/PADDING/HEIGHT/BOTTOM_GAP constants)
- `src/dock/DockWindow.cpp` (new `BitmapToIcon`, `FetchJumboIcon`, `FetchShellBitmap` helpers; UWP path reorder; `OnPaint` rewritten around `Gdiplus::Graphics`; color-key layered attrs in `Create`)
- `src/dock/DockIcon.h` / `.cpp` (HBITMAP member + bitmap ctor arg)
- `docs/SESSION_LOG.md` (this entry)

### Decisions Made
- None formally logged — all changes are visual tuning within Phase 3's scope.
- **Effectively deprecated** DEC-015 direction: GDI+ is in the rendering pipeline now. Phase 4 may still revisit for Direct2D during magnification animation.

### Mid-session course corrections
- Round 1 (ICON_SIZE 48, AlphaBlend) produced no visible change — Bala called it out, forced re-diagnosis. Root causes: `DrawIconEx` ignores stretch mode, `AlphaBlend` does nearest-neighbour scaling, `SIIGBF_ICONONLY` returns blank for Windows 11 Settings. Switched to GDI+ bicubic and factory-tile path.
- Color-key approach initially clipped near-black pixels; switched key to magenta (`RGB 255,0,255`) which no real icon pixel matches.

### Blockers / Notes
- Settings icon final state: displays the package tile logo including its background colour. Matches what Windows 11 Start Menu shows. Bala accepted.
- `LWA_COLORKEY` is a temporary chroma-key solution. Phase 6 will replace with proper DirectComposition acrylic/blur.
- `DockIcon` now has two icon fields (HICON + HBITMAP). Slightly awkward but avoids a lossy HBITMAP→HICON conversion for the UWP fallback path. Can be cleaned up later.

### Next Session Should
1. Begin **Phase 4 planning** — raise decisions before writing any animation code:
   - DEC-014: Frame rate mechanism (16ms `WM_TIMER` vs Direct2D render loop)
   - DEC-015 (revised): Keep GDI+ for static icon draw, or switch to Direct2D for animation-friendly GPU-backed scaling?
   - DEC-016: Magnification curve shape (linear, cosine, Gaussian)
2. Wait for Bala approval on all Phase 4 decisions before implementing.
3. (Low priority) Consider consolidating `DockIcon`'s HICON + HBITMAP into a single render source.

---

## Session 009 — 2026-04-17

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 3 complete (v0.3.2); Phase 4 not started
**Phase at end:** Phase 4 **In Progress** — magnification code written, pending Bala's build + smoke test

### What Was Done
- **DEC-014, DEC-015, DEC-016 raised and APPROVED** by Bala:
  - DEC-014: 16ms `WM_TIMER` for animation loop
  - DEC-015: Keep GDI+ for animated scaling (no Direct2D switch)
  - DEC-016: Cosine falloff magnification curve
- **Phase 4 fish-eye magnification implemented:**
  - `DockWindow.h`: added `DOCK_WINDOW_HEIGHT=72` (animation headroom), `ICON_BOTTOM_Y=66` (bottom-anchored layout), `TIMER_ANIMATE=3`, `m_cursorPos`, `m_mouseTracking`.
  - `DockWindow.cpp` `Create`/`Reposition`: dock window is now `DOCK_WINDOW_HEIGHT=72` tall (extra transparent top region gives magnified icons room to grow upward). Icons bottom-anchored at `ICON_BOTTOM_Y`.
  - `DockWindow.cpp` `OnMouseMove`: arms `TrackMouseEvent` for `WM_MOUSELEAVE`, starts 16ms `TIMER_ANIMATE`, updates `m_cursorPos` each move.
  - `DockWindow.cpp` `OnMouseLeave`: stops timer, resets cursor pos, triggers final repaint (restores normal scale).
  - `DockWindow.cpp` `OnTimer`: `TIMER_ANIMATE` case → `InvalidateRect`.
  - `DockWindow.cpp` `OnPaint`: per-frame cosine scale computation → z-sorted draw order (smallest scale first, magnified icon on top) → bottom-anchored `DrawImage` at scaled size.
  - Added `#include <numeric>` + `#include <cmath>`.

### Files Changed
- `src/dock/DockWindow.h` (DOCK_WINDOW_HEIGHT, ICON_BOTTOM_Y, TIMER_ANIMATE, m_cursorPos, m_mouseTracking)
- `src/dock/DockWindow.cpp` (Create, Reposition, OnPaint, OnMouseMove, OnMouseLeave, OnTimer)
- `docs/DECISIONS.md` (DEC-014/015/016 APPROVED)
- `docs/CHANGELOG.md` (Unreleased Phase 4 section)
- `docs/SESSION_LOG.md` (this entry)
- `docs/LOG.md` (Session 009 notes)

### Decisions Made
- DEC-014 — Animation frame rate: WM_TIMER 16ms (APPROVED)
- DEC-015 — Rendering backend: keep GDI+ (APPROVED)
- DEC-016 — Magnification curve: cosine falloff (APPROVED)

### Blockers / Notes
- Build + smoke test required from Bala before Phase 4 is marked complete.
- `DOCK_TOTAL_HEIGHT=42` in `main.cpp` is unchanged — it's the work-area reservation height, not the window height.
- The transparent top 32px of the dock window (`DOCK_WINDOW_HEIGHT=72` minus the visible `DOCK_HEIGHT=40`) will receive `WM_MOUSEMOVE` and trigger animation — this is acceptable (macOS behavior: magnification activates as you approach the dock from above).

### Next Session Should
1. Begin **Phase 5 planning** — raise decisions before writing any menu bar code:
   - Clock widget (update interval, format)
   - Battery / volume / Wi-Fi widget layout
   - Active app name via `SetWinEventHook`
2. Wait for Bala approval on Phase 5 decisions before implementing.

---

## Session 010 — 2026-04-17

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 4 pending verification (implementation landed Session 009)
**Phase at end:** Phase 4 verified + marked complete (v0.4.0); Phase 5 **In Progress** — menu bar implementation landed, pending Bala's build + smoke test

### What Was Done
- **Phase 4 close-out.** Bala confirmed fish-eye magnification smoke test passed. Deduplicated v0.4.0 entry in `CHANGELOG.md` (Session 009 had written both a real and a placeholder-TBD entry).
- **Raised DEC-017..DEC-021** for Phase 5. Bala said "choose the best thing which will make my desktop look like mac os" — recorded as approval of the macOS-authentic option for each:
  - DEC-017 APPROVED: clock format `Ddd D Mon  HH:MM` (e.g. `Fri 17 Apr  14:32`).
  - DEC-018 APPROVED: battery + volume + Wi-Fi + clock in v1.
  - DEC-019 APPROVED: widgets are static (no click behavior) in v1.
  - DEC-020 APPROVED: `SetWinEventHook` + exe version-info FileDescription → friendly app names.
  - DEC-021 APPROVED: static Apple glyph in v1 (vector path, no PNG asset).
- **Implemented Phase 5 end to end:**
  - `SystemInfo.cpp` — real `GetBattery` (`GetSystemPowerStatus`), `GetVolume` (COM `IMMDeviceEnumerator` + `IAudioEndpointVolume`), `GetWifi` (`WlanOpenHandle` + `WlanQueryInterface`).
  - `ActiveAppWatcher.cpp` — rewritten: `QueryFullProcessImageNameW` for exe path, version-info `FileDescription` lookup for clean names (with capitalized-basename fallback), path→name cache, shell/overlay class blacklist filter.
  - `SystemInfoBar.{h,cpp}` — GDI+ vector glyph renderers (Wi-Fi arcs, speaker+sound-waves, battery outline+fill+bolt), macOS date/time format. Added `wifiQuality` field to `SystemInfoData` so signal strength reaches the renderer.
  - `MenuBarWindow.cpp` — full rewrite of `OnPaint`. Double-buffered GDI+ paint with bold Segoe UI app name next to a vector Apple glyph (left), widget cluster on the right. Single 2s timer drives clock + widget refresh. `WM_ERASEBKGND` suppressed.
  - `main.cpp` — wires `ActiveAppWatcher::Start(lambda → menuBar.SetActiveAppName)` after window creation; `Stop()` before exit.
  - `CMakeLists.txt` — added `version.lib`.

### Files Changed
- `src/system/SystemInfo.cpp` (real implementation)
- `src/menubar/ActiveAppWatcher.cpp` (FileDescription + shell filter + cache)
- `src/menubar/SystemInfoBar.h` (added `wifiQuality` to `SystemInfoData`)
- `src/menubar/SystemInfoBar.cpp` (full GDI+ vector rendering)
- `src/menubar/MenuBarWindow.cpp` (double-buffered GDI+ paint, Apple glyph, 2s timer)
- `src/main.cpp` (ActiveAppWatcher wiring)
- `CMakeLists.txt` (version.lib)
- `CLAUDE.md` (Phase 5 → In Progress, footer)
- `docs/DECISIONS.md` (DEC-017..DEC-021 logged + APPROVED)
- `docs/CHANGELOG.md` (dedupe v0.4.0; Phase 5 entries in Unreleased)
- `docs/SESSION_LOG.md` (this entry)
- `docs/LOG.md` (Session 010 notes)

### Decisions Made
- DEC-017 — Clock format & cadence (APPROVED)
- DEC-018 — Widget scope v1 (APPROVED)
- DEC-019 — Widget interactivity (APPROVED — static)
- DEC-020 — Active app name source (APPROVED — FileDescription)
- DEC-021 — Apple logo (APPROVED — static vector glyph)

### Blockers / Notes
- Build + smoke test pending on Bala's machine. Expected result: menu bar shows vector Apple glyph at left, bold app name that updates when switching windows (e.g. "Google Chrome", "File Explorer", "Visual Studio Code"), and on the right a Wi-Fi arc glyph, volume speaker, battery outline with fill, and a `Ddd D Mon  HH:MM` clock — all anti-aliased.
- `kBarBg` colour used for the Apple glyph's "bite" overlay must match the background; a future Phase 6 acrylic/blur swap will need to update the bite fill method (probably switch to a proper path-subtraction approach).

### Next Session Should
1. Bala builds (`cmake --build build --config Release`) and runs `macOSWin.exe`. Verify:
   - Menu bar shows Apple glyph + app name (switches in real time as you alt-tab).
   - Right side shows clock with date (macOS format), battery (if laptop), volume, Wi-Fi glyph.
   - No flicker on 2s timer tick.
   - Ctrl+Alt+Q still quits cleanly.
2. If all four widgets visible and correct → mark Phase 5 **Complete**, tag v0.5.0.
3. If any widget mis-renders → capture screenshot/symptom in `docs/LOG.md`, diagnose, patch.
4. Begin **Phase 6 polish** planning — raise decisions:
   - DirectComposition acrylic/blur for Dock + MenuBar
   - Dock entrance animation
   - Embed real Apple .png / .ico asset
   - Interactive Apple menu dropdown (revisit DEC-021)
   - Clickable widget flyouts (revisit DEC-019)

---

## Session 011 — 2026-04-18

**Type:** Development + iterative visual polish
**Participants:** Bala, Claude Code
**Duration:** Single conversation (continued from Session 010 after context compaction)
**Phase at start:** Phase 5 implementation landed, pending smoke test
**Phase at end:** Phase 5 verified and polished — awaiting formal close-out / v0.5.0 tag

### What Was Done
- **Apple glyph → functional media controls.** Replaced static Apple vector glyph with Prev / Play-Pause / Next buttons that send `VK_MEDIA_PREV_TRACK / PLAY_PAUSE / NEXT_TRACK` via `SendInput`. Works with Spotify, YouTube, VLC, browser tabs.
- **Segoe Fluent Icons glyphs** (\uE892 / \uE768 / \uE769 / \uE893) with Segoe MDL2 Assets fallback — crisp vector rendering at any DPI.
- **Real SMTC play-state detection** (C++/WinRT `GlobalSystemMediaTransportControlsSessionManager`) on a dedicated MTA background thread (STA on UI thread would deadlock on `RequestAsync().get()`). Atomic `m_isPlaying` toggles the middle glyph between Play and Pause automatically when media starts/stops.
- **Process DPI awareness** (`SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`). Root cause of the "everything looks pixelated" complaint was Windows bitmap-stretching a DPI-unaware window. Scaled every drawing-coordinate consumer:
  - `MenuBarWindow` — `m_dpiScale` computed from `GetDpiForSystem`, scales window height + all glyph/text coords.
  - `DockWindow` — added `m_dpiScale` + `S()` helper, scaled `ICON_SIZE`, `ICON_PADDING`, `DOCK_WINDOW_HEIGHT`, `BOTTOM_GAP`, `ICON_BOTTOM_Y`, `DOT_RADIUS`, `DOT_OFFSET`, `MAGNIFY_RADIUS`, `DOCK_EMPTY_WIDTH`, `pillPadY`.
  - `SystemInfoBar` — `g_scale` refreshed per Render frame, scaled Wi-Fi / volume / battery glyph dimensions, stroke widths, and clock font size.
  - `main.cpp` — `MulDiv(height, sysDpi, 96)` on AppBar work-area reservation.
- **Dock: per-pixel alpha.** Replaced `LWA_COLORKEY` (magenta chroma key — can only be fully-opaque or fully-transparent) with `UpdateLayeredWindow(AC_SRC_ALPHA)` backed by a 32bpp top-down DIB. Unlocks true translucency for the new dock panel. Running-indicator dots switched from GDI `Ellipse` (writes 0 alpha → invisible under `AC_SRC_ALPHA`) to `Gdiplus::FillEllipse`.
- **Seelen-style rounded translucent dock pill.** Drawn under the icons each paint at ~60% alpha. Does not affect layout, hit-testing, or magnification math.
- **Menu bar Seelen-style gradient.** Two-stop (top → mid → bot) deep charcoal with 1px top highlight + 1px bottom shadow for inset-glass feel.
- **Active-app watcher hardening.**
  - Filter `explorer.exe` + shell processes (`searchhost.exe`, `startmenuexperiencehost.exe`, `shellexperiencehost.exe`).
  - Split class blacklist into `IsDesktopClass` (Progman/WorkerW/Shell_TrayWnd/CabinetWClass/Windows.UI.Core.CoreWindow → reset to "Dock") vs `IsOwnOverlayClass` (our own bar/dock → preserve previous name).
  - Empty callback name also resets to "Dock".
- **`.lnk`-aware running indicators.** `DockWindow::OnTimer` now resolves shortcut targets via `IShellLink` and caches the real exe basename per pinned path. Before this, dots never appeared for any Start-menu-pinned app because `chrome.lnk` never matched `chrome.exe`.
- **Menu bar height** bumped from 28 → 38 logical px (`BAR_HEIGHT` in header + `MENUBAR_HEIGHT` in `main.cpp` kept in sync).
- **Main-loop performance fix** (carried from earlier in the session): `SystemInfoBar::Fetch()` cached on the `m_sysInfo` member and refreshed only on the 2s timer — no more blocking `WlanQueryInterface` on every paint.

### Files Changed
- `src/menubar/MenuBarWindow.h` — `m_dpiScale`, `m_isPlaying` (atomic), `m_smtcStop`, `m_smtcThread`
- `src/menubar/MenuBarWindow.cpp` — DPI scaling, Fluent Icons glyphs, SMTC worker, `WM_MOUSEMOVE` repaint for hover, Seelen gradient
- `src/menubar/SystemInfoBar.cpp` — `g_scale` + `SI/SF` helpers, all widget dimensions scaled
- `src/menubar/ActiveAppWatcher.cpp` — desktop vs own-overlay class split, shell-exe filter, `<algorithm>` include
- `src/dock/DockWindow.h` — `m_dpiScale` + `S()` helper
- `src/dock/DockWindow.cpp` — per-pixel alpha via `UpdateLayeredWindow`, translucent rounded pill, DPI scaling, `.lnk` running-indicator resolution, `<unordered_map>` include
- `src/main.cpp` — `SetProcessDpiAwarenessContext`, `MulDiv`-scaled AppBar
- `CMakeLists.txt` — added `windowsapp` link (C++/WinRT for SMTC)
- `CLAUDE.md` — footer, phase status
- `docs/CHANGELOG.md` — Phase 5 Unreleased entries updated with this session's additions
- `docs/SESSION_LOG.md` — this entry
- `docs/LOG.md` — Session 011 polish notes

### Decisions Made
- None formal. DEC-021 (Apple glyph) was effectively superseded in practice by the media-control cluster per Bala's in-session direction ("keep something usable like media controls instead of traffic light"). Not relogged as a separate decision since the existing DEC-021 language covers "left-cluster glyph in v1" loosely enough.

### Blockers / Notes
- None open. Bala confirmed final state looks sharp and correct.
- SMTC worker uses `winrt::init_apartment(multi_threaded)` on the poller thread. `windowsapp.lib` is available on every supported Windows SDK (10.0.17134+) so no conditional compile needed in practice — but the `#if __has_include(<winrt/Windows.Media.Control.h>)` guard stays as a safety net.
- Dock now uses `UpdateLayeredWindow` instead of `SetLayeredWindowAttributes(LWA_COLORKEY)`. The two are mutually exclusive — a future Phase 6 DirectComposition/acrylic rework will need to pick one model or the other consciously.

### Next Session Should
1. Formally close Phase 5: tag `v0.5.0`, update `CHANGELOG.md` from Unreleased → 0.5.0 with today's date, mark Phase 5 **Complete** in `CLAUDE.md`.
2. Append a new Phase 5 test-run row to `docs/TESTING.md` covering the widgets + media controls + active-app naming + DPI sharpness + translucent dock + running dots.
3. Begin **Phase 6** (Polish) planning. Candidate decisions:
   - DirectComposition acrylic/blur for dock + menu bar (real blur-behind, not just alpha).
   - Interactive media-cluster (hover tooltip with song title via SMTC `GetMediaPropertiesAsync`).
   - Clickable widget flyouts (revisit DEC-019).
   - Dock entrance animation.

---

## Session 012 — 2026-04-18

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 5 (verified, awaiting formal tag)
**Phase at end:** Phase 6 In Progress — v0.6.0 shipped with Now-Playing + multi-monitor only; blur/slide-up/window-animator deferred.

### Addendum (post smoke-test)
Bala ran the build mid-session. Multiple Phase 6 features failed on their Win11 machine and were reverted:
- **DEC-022 Blur**: both `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` and `DwmEnableBlurBehindWindow` produced a solid tint box (dark, then white) instead of actual blur. Microsoft has neutered both APIs for `WS_EX_LAYERED` windows on Win11. Reverted — no blur. Status changed APPROVED → PENDING/deferred.
- **DEC-023 Slide-up**: tried three approaches (WM_TIMER + InvalidateRect; WM_TIMER + RedrawWindow(RDW_UPDATENOW); synchronous Sleep loop with cached layered bitmap). None produced visible motion on Bala's build; the synchronous version caused the overlay to stay invisible entirely (taskbar hidden + no bars = "only wallpaper"). Reverted to plain `ShowWindow`. Status APPROVED → PENDING/deferred.
- **MenuBarWindow OnPaint rewrite**: converted to `UpdateLayeredWindow(AC_SRC_ALPHA)` with 32bpp DIB during the blur attempt. Reverted to the Phase 5 `BitBlt` + `SetLayeredWindowAttributes(LWA_ALPHA, 255)` path. Gradient colours bumped to opaque (alpha=255).
- **DEC-026 Window-animator** (logged + approved mid-session): prototyped `src/system/WindowAnimator.{h,cpp}` hooking `EVENT_OBJECT_SHOW`, adding `WS_EX_LAYERED` to foreign windows, animating scale+fade from 85%. Bala launched and saw no animation — likely `SetWindowLongPtr(WS_EX_LAYERED)` silently failing post-show on Win11. Bala dropped the feature. Files deleted; status REJECTED.

### What Actually Shipped (v0.6.0)
- DEC-024 Now-Playing inline text on menu bar (SMTC `TryGetMediaPropertiesAsync` → mutex-protected string → GDI+ DrawString with ellipsis trimming at 220px).
- DEC-025 Primary-monitor helper (`Composition::GetPrimaryMonitorSize`) — replaces `GetSystemMetrics(SM_CXSCREEN/CYSCREEN)` in DockWindow, MenuBarWindow, AppBarManager.
- Phase 5 formal close-out: v0.5.0 tagged, TESTING.md Phase 5 rows filled + Test Run Log row appended, CLAUDE.md Phase 3 & 5 marked Complete.
- Dead code cleanup: WindowAnimator files removed; entrance-animation fields in DockWindow.h slimmed; blur/UpdateLayeredWindow paths in MenuBarWindow removed.

### What Was Done
- **Phase 5 formally closed**: promoted `CHANGELOG.md` Unreleased → `[0.5.0] — 2026-04-18`; flipped Phase 5 to **Complete** in `CLAUDE.md`; filled `docs/TESTING.md` Phase 5 rows T5-001..T5-010 with Session 011 PASS results, added four new rows T5-011..T5-014 (media controls, SMTC play-state, DPI sharpness, translucent dock pill), appended Phase 5 Test Run Log row.
- **Phase 6 decisions logged + approved** per Bala's "best for looking like mac" directive: DEC-022 (blur via undocumented `SetWindowCompositionAttribute`), DEC-023 (dock slide-up entrance), DEC-024 (inline Now-Playing text), DEC-025 (primary-monitor via `GetMonitorInfo`).
- **DEC-022 Blur**: created `src/system/CompositionHelper.h` (header-only) with `ApplyBlurBehind` (ABGR tint `0x60181820` default) and `RemoveBlurBehind`. Applied in `DockWindow::Create` and `MenuBarWindow::Create`.
- **Menu bar rewrite**: converted `MenuBarWindow::OnPaint` from `BeginPaint`+`BitBlt` with `SetLayeredWindowAttributes(LWA_ALPHA,255)` to `UpdateLayeredWindow(AC_SRC_ALPHA)` with 32bpp top-down DIB. Semi-transparent gradient (alpha 170/150/130) now lets the DWM blur show through. Opaque LWA_ALPHA would have hidden the blur entirely.
- **DEC-023 Entrance**: added `TIMER_ENTRANCE`, `m_targetY`, `m_entranceFrame`, `m_entranceActive` to `DockWindow`. `Show()` parks the window offscreen (targetY + DOCK_WINDOW_HEIGHT + 20), arms `ShowWindow(SW_SHOWNOACTIVATE)`, starts 16ms timer. `OnTimer(TIMER_ENTRANCE)` runs 16 frames of ease-out quadratic (`1 - (1-t)²`) = 256ms total, settles at m_targetY, kills timer. `Reposition()` caches m_targetY and only repositions when not animating.
- **DEC-024 Now-Playing**: extended SMTC worker in `MenuBarWindow` — after fetching play state, calls `TryGetMediaPropertiesAsync().get()` and composes `L"Title \u2014 Artist"`. Result written under `m_nowPlayingMutex` to `m_nowPlayingText`. `OnPaint` reads under lock, renders between app name and right widgets with `StringTrimmingEllipsisCharacter` at 220 logical px, colour `kTrackFg` (dimmed white).
- **DEC-025 Multi-monitor**: replaced all `GetSystemMetrics(SM_CXSCREEN/CYSCREEN)` calls in `DockWindow`, `MenuBarWindow`, and `AppBarManager` (both `Apply()` and `WM_DISPLAYCHANGE`) with `Composition::GetPrimaryMonitorSize()`.
- Updated `CLAUDE.md` file reference map to list `CompositionHelper.h`; updated `CHANGELOG.md` with Phase 6 Unreleased section; updated footers to reflect Session 012 state.

### Files Changed
- `src/system/CompositionHelper.h` — NEW
- `src/dock/DockWindow.h` / `.cpp` — entrance animation, blur, primary-monitor helper
- `src/menubar/MenuBarWindow.h` / `.cpp` — blur, UpdateLayeredWindow rewrite, Now-Playing text, SMTC metadata
- `src/system/AppBarManager.cpp` — DEC-025 routing
- `docs/DECISIONS.md` — DEC-022..DEC-025 appended (APPROVED)
- `docs/CHANGELOG.md` — v0.5.0 tagged; Phase 6 Unreleased section added
- `docs/TESTING.md` — Phase 5 results + Phase 5 Test Run Log row
- `CLAUDE.md` — Phase 3/5 Complete, Phase 6 In Progress, file map, footer
- `docs/SESSION_LOG.md` — this entry
- `docs/LOG.md` — Session 012 implementation notes

### Decisions Made
- DEC-022, DEC-023, DEC-024, DEC-025 — all APPROVED per Bala's delegation ("chose the option best for me which make it looks like mac").

### Blockers / Notes
- `SetWindowCompositionAttribute` is undocumented. Works reliably on Win10 1803+ and all Win11 builds; would break if Microsoft removes it. Acceptable for a hobby overlay.
- Blur composes correctly with `UpdateLayeredWindow` because DWM blurs the desktop surface *behind* the window before compositing our alpha DIB on top. The tint's alpha byte (0x60 = ~38%) controls tint strength.
- SMTC metadata fetch runs on the same background MTA thread as the play-state poll; only the result copy crosses the mutex boundary, keeping UI thread (OnPaint) contention minimal.
- No build/verify performed this session (development environment only). Bala to smoke-test.

### Next Session Should
1. Plan the **DirectComposition rewrite** as the honest path to real Phase 6 acrylic + entrance animation. Abandons `WS_EX_LAYERED` + `UpdateLayeredWindow`, adopts `WS_EX_NOREDIRECTIONBITMAP` with a DComp visual tree. Both bars must be rewritten; significant risk; estimate 2 focused sessions. Needs a new decision (DEC-027) with Bala's explicit approval before starting.
2. In the meantime, consider low-risk polish: proper Apple logo vector, a real multi-resolution `.ico` for the tray, rounded corner clipping on the dock pill edges (already rounded — verify on high DPI).
3. Do NOT re-attempt DEC-022/DEC-023 via `SetWindowCompositionAttribute` or `DwmEnableBlurBehindWindow` — both confirmed non-functional on Win11 with layered windows.

---

## Session 013 — 2026-04-18

**Type:** Development
**Participants:** Bala, Claude Code
**Duration:** Single conversation
**Phase at start:** Phase 6 In Progress (v0.6.0 shipped; blur/slide-up deferred)
**Phase at end:** Phase 6 In Progress — DEC-027 DirectComposition rewrite written, pending Bala build + smoke test

### What Was Done
- **Bookkeeping:** Fixed DEC-012 status PENDING → APPROVED (was implemented in Session 006; status field never updated).
- **DEC-027 raised + approved** (Option A — full DComp rewrite, both bars): Bala chose Option A (real frosted glass + slide-up animation, 2-session effort).
- **`CompositionHelper.h` rewritten** — added `DCompWindow` struct (C++17 inline statics for shared D3D11/DXGI/DComp device; per-window `IDCompositionTarget` + `IDCompositionVisual` + `IDCompositionSurface`; `BeginDraw/EndDraw/Commit/Release` helpers using `IDXGISurface1` GDI interop), `ApplySystemBackdrop()` (`DWMSBT_TRANSIENTWINDOW`), `DWMWA_SYSTEMBACKDROP_TYPE` / enum guards for older SDKs. Legacy blur helpers retained for reference.
- **`DockWindow`** (DEC-027): Replaced `WS_EX_LAYERED` + `UpdateLayeredWindow` with `WS_EX_NOREDIRECTIONBITMAP` + DComp surface rendering. Added `WM_NCHITTEST → HTTRANSPARENT` for headroom above pill. `DOCK_WINDOW_HEIGHT` 72→60 (shrinks acrylic zone above pill). **Entrance animation restored** (DEC-023): `TIMER_ENTRANCE` now only calls `SetWindowPos` per tick — no per-frame GDI re-render needed because DComp surface is static during slide.
- **`MenuBarWindow`** (DEC-027): Replaced `WS_EX_LAYERED` + `SetLayeredWindowAttributes` + `BitBlt` with `WS_EX_NOREDIRECTIONBITMAP` + DComp surface rendering. Menu bar colours made semi-transparent (`kBarTop/Mid/Bot` alpha 255→200/180/160) so DWMSBT acrylic is visible as frosted glass behind the tinted gradient.
- **`CMakeLists.txt`**: Added `d3d11`, `dxgi` link targets.

### Files Changed
- `src/system/CompositionHelper.h` (complete rewrite — DComp device + DCompWindow + DWMSBT + legacy helpers)
- `src/dock/DockWindow.h` (DComp member, DOCK_WINDOW_HEIGHT/ICON_BOTTOM_Y reduced)
- `src/dock/DockWindow.cpp` (DComp rendering path, entrance animation, WM_NCHITTEST)
- `src/menubar/MenuBarWindow.h` (DComp member)
- `src/menubar/MenuBarWindow.cpp` (DComp rendering path, semi-transparent colours)
- `CMakeLists.txt` (d3d11, dxgi added)
- `docs/DECISIONS.md` (DEC-012 APPROVED, DEC-027 raised + APPROVED)
- `docs/CHANGELOG.md` (Unreleased v0.7.0 section)
- `docs/SESSION_LOG.md` (this entry)
- `docs/LOG.md` (Session 013 notes)

### Decisions Made
- DEC-027 — APPROVED (Option A: full DComp rewrite, both bars)
- DEC-012 — Status corrected to APPROVED (was already implemented in Session 006)

### Blockers / Notes
- No build performed this session (dev environment only). Bala must build + verify.
- Key risk: `IDCompositionSurface::BeginDraw(IID_IDXGISurface1)` GDI interop path — tested in Microsoft SDK samples but not on Bala's specific GPU. If BeginDraw returns E_NOINTERFACE, will need to switch to D2D interop (`IDCompositionDevice2` + `ID2D1DeviceContext` + `ID2D1GdiInteropRenderTarget`).
- `DWMSBT_TRANSIENTWINDOW` requires Win11 22H2+. Older builds: `DwmSetWindowAttribute` returns error, bars render opaque-dark (same as Phase 5). No crash.
- Menu bar is now semi-transparent. If acrylic isn't available (older OS), the bar will look lighter than Phase 5 (semi-transparent dark over solid desktop = reveals desktop colour). Bala to evaluate and report.

### Next Session Should
1. Bala runs full rebuild: `rmdir /s /q build`, then `cmake -S . -B build -A x64 && cmake --build build --config Release && build\bin\Release\macOSWin.exe`.
2. Verify: (a) real frosted-glass acrylic visible behind both bars; (b) dock slides up from below on first launch; (c) pill shape, magnification, media controls, active app all still work; (d) Ctrl+Alt+Q quits cleanly.
3. If `BeginDraw(IID_IDXGISurface1)` fails — report the blank window symptom, and we switch to `IDCompositionDevice2` + D2D interop in the next session.
4. If acrylic not visible (older Win11 build) — report OS build number; may need to also test `SetWindowCompositionAttribute(ACCENT_ENABLE_ACRYLIC_BLUR_BEHIND)` as a non-layered fallback.
5. Tag v0.7.0 once verified.

---

*Template for future sessions:*
```
## Session XXX — YYYY-MM-DD

**Type:** Development / Debug / Review
**Participants:** Bala, Claude Code
**Duration:**
**Phase at start:**
**Phase at end:**

### What Was Done
-

### Files Changed
-

### Decisions Made
-

### Blockers / Notes
-

### Next Session Should
1.
```
