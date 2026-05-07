# Development Log — macOS Windows Overlay

> Running notes, findings, blockers, and implementation observations.
> Always append — never edit past entries. Tag each entry with date and session number.

---

## 2026-05-07 | Session 015 — Config migration + startup toggle

### DEC-028: Config location → %APPDATA%
- `GetConfigPath()` now calls `SHGetFolderPathW(CSIDL_APPDATA)` → `%APPDATA%\macOSWin\pinned_apps.json`.
- `CreateDirectoryW` called each time (idempotent) to ensure dir exists.
- `MigrateLegacyConfig()` runs on every `Load()`: if new path doesn't exist but old next-to-exe path does, copies it over via `CopyFileW(src, dst, TRUE)` (bFailIfExists=TRUE prevents overwriting).
- Falls back to legacy path if `SHGetFolderPathW` fails (unlikely but safe).
- Existing gtest suite uses `GetConfigPath()` dynamically — tests automatically run against the new location.

### DEC-029: Run at startup
- Tray right-click menu now has "Run at startup" (checked/unchecked) above the separator and Quit.
- `IsStartupEnabled()` checks `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` for value `macOSWin`.
- `SetStartupEnabled(true)` writes the current exe path as `REG_SZ`.
- `SetStartupEnabled(false)` deletes the value.
- `advapi32.lib` added to CMake link targets for registry APIs.

### Crash recovery
- Sentinel file `%APPDATA%\macOSWin\.running` — created on startup, deleted on clean exit.
- `OnStartup()` checks for stale sentinel → restores taskbar if previous instance crashed.
- `SetUnhandledExceptionFilter` catches unhandled exceptions → restores taskbar + deletes sentinel.
- Does NOT catch `taskkill /F` (SIGKILL equivalent) — in that case the sentinel persists and the next launch cleans up.

### Dead code cleanup
- Removed `DockDropHandler.h/.cpp` — dead since Session 006 (OLE `DockDropTarget` replaced it).
- Removed `d3d11`, `dxgi`, `dcomp` from CMake link targets — unused since DComp rewrite was reverted.
- Cleaned stale DComp comment in `DockWindow.h`.

### Display-change robustness
- Already handled: `DockWindow`, `MenuBarWindow`, and `AppBarManager` all respond to `WM_DISPLAYCHANGE`. No additional work needed.

### Open items carried forward
- [ ] Phase 6: DirectComposition acrylic/blur (deferred)
- [ ] Phase 6: Dock entrance slide-up animation (deferred)
- [ ] Phase 6: Tray icon asset (replace IDI_APPLICATION placeholder)

---

## 2026-05-07 | Session 014 — Dock invisibility fix

### Root cause
With `UpdateLayeredWindow(AC_SRC_ALPHA)`, pixels with alpha=0 are invisible. The dock pill was only drawn when `m_icons.size() > 0`. An empty dock (no `pinned_apps.json` or empty config) produced an entirely transparent surface — the dock window existed but was invisible. The menu bar was unaffected because it always draws a full gradient.

### Fix
Changed `if (n > 0 && !m_flashActive)` to `if (!m_flashActive)` in `RenderDComp()`. Empty dock now shows a pill spanning the full `DOCK_EMPTY_WIDTH`. With icons, the pill wraps around them as before.

### Open items carried forward
- [ ] Phase 6: DirectComposition acrylic/blur (fresh approach needed — DEC-027 DComp path reverted)
- [ ] Phase 6: Dock entrance slide-up animation
- [ ] Phase 6: Real Apple logo asset, tray icon asset
- [ ] Config location (`%APPDATA%` vs next-to-exe)
- [ ] Run-at-Windows-startup decision (long deferred)

---

## 2026-04-18 | Session 013 — DEC-027 DirectComposition rewrite

### Architecture
- Both bars now use `WS_EX_NOREDIRECTIONBITMAP` (not `WS_EX_LAYERED`). DWM no longer creates a GDI redirection surface; all content goes through the DComp visual tree.
- `DWMSBT_TRANSIENTWINDOW` (= `DWMWA_SYSTEMBACKDROP_TYPE 38`) applied after window creation. Requires Win11 22H2+ (build 22621). Returns S_OK on supported builds, E_INVALIDARG or similar on older — we check nothing and fall through gracefully (window renders without acrylic blur).
- Shared D3D11 device: `D3D11_CREATE_DEVICE_BGRA_SUPPORT` is mandatory for `IDXGISurface1::GetDC` (GDI interop). Falls back to WARP (software renderer) if hardware device creation fails.

### DComp surface GDI interop pattern
`IDCompositionDevice::CreateSurface` (DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED) → `BeginDraw(nullptr, IID_IDXGISurface1, &surf, &offset)` → `surf->GetDC(FALSE, &hdc)` → GDI+ rendering → `surf->ReleaseDC(nullptr)` → `surf->Release()` → `surface->EndDraw()` → `device->Commit()`. For non-virtual surfaces, `offset` is always {0,0}.

### Premultiplied alpha note
The DComp surface expects premultiplied BGRA. GDI+ writes straight alpha. For the dock pill (dark, ~60% alpha) and menu bar gradient (~70-80% alpha), the premultiplication error (RGB not pre-divided by alpha) is small and visually imperceptible at these near-black values — same as the previous UpdateLayeredWindow path.

### Entrance animation
`TIMER_ENTRANCE` now calls only `SetWindowPos` per tick (no GDI re-render). DComp surface content is static during the slide — the compositor handles compositing each frame. This avoids the per-frame UpdateLayeredWindow cost that caused the previous animation to fail on Win11.

### WM_NCHITTEST for dock transparency
Returns `HTTRANSPARENT` for y < pillT (the transparent headroom above the pill). Pixels with alpha=0 in the DComp visual DON'T automatically pass mouse events in Win32 — explicit HTTRANSPARENT is required.

### Menu bar alpha
Changed `kBarTop/Mid/Bot` from alpha=255 (opaque) to 200/180/160 (semi-transparent). The DWMSBT acrylic shows through where alpha < 255, creating the frosted-glass look. Fully opaque pixels would cover the acrylic entirely (previous opaque appearance = no blur visible even with DWMSBT set).

### Open items carried forward
- [ ] Bala to build and smoke-test. Key things to verify: (1) real blur visible behind both bars on Win11 22H2+; (2) slide-up animation fires on first launch; (3) dock icons still render with magnification; (4) media controls + active app still update.
- [ ] If DWMSBT not supported (older Win11 build), bars will render opaque-dark as before — no crash.
- [ ] `IDCompositionSurface::BeginDraw(IID_IDXGISurface1)` — confirm GDI interop works on Bala's GPU. If it fails silently (returns non-null HDC but draws nothing), may need to switch to D2D interop path.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Tray icon asset (Phase 6 remaining polish).

---

## 2026-04-18 | Session 011 — Phase 5 polish

### SMTC (System Media Transport Controls) via C++/WinRT
- `GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get()` is blocking. **Must not run on the UI thread** — UI thread is STA (OleInitialize) and would deadlock waiting on the async op's completion callback.
- Fix: dedicated worker thread, `winrt::init_apartment(multi_threaded)`, 2s poll cadence with 100ms-granularity interrupt via `std::atomic<bool> m_smtcStop`. State surfaced through `std::atomic<bool> m_isPlaying`. UI thread reads the atomic; worker triggers `InvalidateRect` on state transition.
- `windowsapp.lib` is the single import lib for the whole C++/WinRT runtime. No /await flag needed because `.get()` on `IAsyncOperation` is synchronous.

### Per-pixel-alpha dock window
- `SetLayeredWindowAttributes(LWA_COLORKEY)` and `UpdateLayeredWindow(AC_SRC_ALPHA)` are mutually exclusive. Calling both produces undefined visual state.
- `UpdateLayeredWindow` expects **premultiplied** BGRA. GDI+ drawing into a 32bpp top-down DIB section produces premult-compatible output for `DrawImage` / `FillPath` / `FillEllipse`. GDI calls (`Ellipse`, `FillRect`, `BitBlt`) write `alpha=0`, which makes them invisible under `AC_SRC_ALPHA` — converted all dock drawing to GDI+.
- `UpdateLayeredWindow` takes the window's ABSOLUTE screen position via `ptDst`. Use `GetWindowRect`, not `GetClientRect`, for the positioning vector.
- Input still routes to the dock (not pass-through) for any pixel with alpha > 0, which covers the whole pill region. Fully transparent pixels (outside the pill) pass mouse events through to the desktop, which is the macOS dock behavior we want.

### DPI pitfalls
- Process-level DPI awareness (`SetProcessDpiAwarenessContext`) must be set BEFORE any window is created. After window creation, it silently no-ops.
- Enabling DPI awareness without scaling drawing coordinates makes every window render at physical-pixel size — visibly *smaller* on high-DPI displays (the dock shrinking we saw mid-session). Every constant that participates in layout must be scaled by `GetDpiForSystem() / 96`.
- `GetDpiForWindow` works per-monitor; `GetDpiForSystem` is good enough for a single-monitor start. If multi-monitor support is added in Phase 6, consider reacting to `WM_DPICHANGED`.

### `.lnk` running-indicator resolution
- `DragAcceptFiles` / config loader stores the raw `.lnk` path. `ProcessMonitor` returns running exe basenames, so a direct basename compare was always failing for Start-menu-pinned apps.
- Resolved via `IShellLinkW::GetPath` once per pin, cached in a `static std::unordered_map<std::wstring, std::wstring>` keyed by the original pin path. COM is already initialized (`OleInitialize` in WinMain) so `CoCreateInstance(CLSID_ShellLink)` works without extra setup.

### Active-app name — "Dock" as the idle state
- Filtering shell classes (Progman/WorkerW) without an explicit reset caused the bar to freeze on the last real app when focus returned to the desktop (visible as a stale "Claude" label when the desktop was the actual active surface).
- Split filter into two sets: `IsDesktopClass` → emit empty string → `SetActiveAppName(L"")` → displays "Dock"; `IsOwnOverlayClass` → skip entirely (don't change display).

---

## 2026-04-14 | Session 001 — Planning

### Project Initialized
- Full planning session completed with Bala via Cowork.
- All 8 architectural decisions logged and approved (see DECISIONS.md).
- Project scaffolding created.

### Notes for Phase 1 (Skeleton)
- To hide the Windows taskbar: `FindWindow(L"Shell_TrayWnd", NULL)` + `ShowWindow(hwnd, SW_HIDE)`. On Windows 11 also need to handle `Shell_TrayWnd` secondary bar.
- For always-on-top borderless windows: use `WS_POPUP` window style with no `WS_CAPTION`. Set topmost via `SetWindowPos(hwnd, HWND_TOPMOST, ...)`.
- Menu bar should span full screen width. Use `GetSystemMetrics(SM_CXSCREEN)` for width.
- Dock window position: `(screenWidth - dockWidth) / 2` for horizontal centering. Add a bottom gap (e.g., 8px) from screen bottom.

### Notes for Phase 2 (Dock Core)
- Icon extraction: `SHGetFileInfo` with `SHGFI_ICON | SHGFI_LARGEICON` is more reliable than `ExtractAssociatedIcon` for `.lnk` shortcut files. Returns the target app's icon.
- `ShellExecute` handles both `.exe` and `.lnk` files uniformly — no need to resolve the shortcut manually for launching.
- To bring a running app to front: `SetForegroundWindow` requires the calling process to be the foreground process or have `AttachThreadInput`. May need workaround — log result when implementing.

### Notes for Phase 3 (Interaction)
- `DragAcceptFiles(hwnd, TRUE)` + handling `WM_DROPFILES` is the simplest approach for drag-to-pin.
- `DragQueryFile` retrieves the dropped file path. Can drop multiple files — handle each one.
- Running indicator: poll every 1500ms with `EnumWindows`. Avoid polling every frame — too expensive.

### Notes for Phase 4 (Animation)
- Magnification effect: track `WM_MOUSEMOVE` globally across the Dock. For each icon, compute distance from cursor to icon center. Apply scale = `lerp(1.0, 1.8, 1.0 - clamp(dist / 120, 0, 1))`.
- Use a `WM_TIMER` at ~16ms (60fps) to drive animation frame redraws during hover.
- Direct2D `ID2D1Bitmap` with `DrawBitmap` supports scaling via destination rect — no shader needed.

### Notes for Phase 5 (Menu Bar)
- `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ...)` fires when the user switches active window. Callback receives `hwnd` — use `GetWindowText` for window title and `GetModuleFileNameEx` for process path.
- Battery: `GetSystemPowerStatus` — simple struct, no COM required.
- Volume: requires COM — `CoCreateInstance` of `MMDeviceEnumerator` → `IMMDeviceEnumerator::GetDefaultAudioEndpoint` → `IAudioEndpointVolume`. Initialize COM in the main thread.
- Wi-Fi: `WlanOpenHandle` → `WlanQueryInterface` with `wlan_intf_opcode_current_connection`. Returns SSID and signal quality.

### Open Items
- [x] nlohmann/json.hpp — handled automatically via CMake FetchContent on first `cmake -S . -B build` run
- [ ] Decide: should the app run at Windows startup? (Add to startup registry?) — log as decision when ready
- [ ] Decide: should unpin be via right-click context menu or drag off the Dock? — log as decision when ready

---

## 2026-04-14 | Session 002 — Phase 1 Implementation

### Findings at session start
- Phase 1 skeleton code was already present in the Session 001 scaffolding (both window classes, `TaskbarManager`, `main.cpp` wiring, `CMakeLists.txt`).
- Session 002 scope reframed from "implement Phase 1" to "harden + verify Phase 1".

### Code changes
- `src/main.cpp` — rewrote to use RAII guards (`TaskbarHideGuard`, `ComApartment`). Guarantees taskbar is restored on any exit path (normal return, early-return from window creation failure, unwinding). Removed dead `WndProc` forward declaration. Added return-code propagation (`return 1` on window creation failure).

### Observations / Tech Debt
- Both `DockWindow::WndProc` and `MenuBarWindow::WndProc` call `PostQuitMessage(0)` on `WM_DESTROY`. In the current build there is no user-facing close action so this is benign, but once we add a close path (tray-icon Quit, DEC to be raised) this needs to be routed through a single "primary" window so closing one does not kill the app prematurely. Track for Phase 2/5.
- Dock layered alpha is set to 240 and MenuBar to 230 as placeholders. Real compositing arrives in Phase 6 via DirectComposition.
- CMake `FetchContent_Declare` uses `DOWNLOAD_NO_EXTRACT TRUE` to grab a single `json.hpp`. `FetchContent_MakeAvailable` on a payload with no `CMakeLists.txt` is silently a no-op on CMake ≥ 3.24. If build fails with an "add_subdirectory" error on older CMake, switch to `FetchContent_Populate` explicitly. Flag if it happens.
- Build tooling (`cmake`, `cl.exe`) not available in the Claude shell — build verification must be run by Bala in a VS Developer Command Prompt.

### Build + Smoke Test Instructions (for Bala)
From a Visual Studio "Developer Command Prompt for VS 2022":
```
cd "C:\Users\yudhi\OneDrive\Documents\windows to macos converter"
cmake -S . -B build -A x64
cmake --build build --config Release
build\bin\Release\macOSWin.exe
```
Expected: taskbar disappears, a thin black bar appears at the top of the screen with a placeholder glyph/app name/clock, and a small black bar appears centered at the bottom. Kill via Task Manager (no quit UI yet). Taskbar should return immediately on exit.

### Open items carried forward
- [x] Phase 1 build + smoke test — PASSED 2026-04-14
- [ ] DEC-009 (to be created): Quit mechanism — tray icon? hotkey? Must be decided before Phase 2.
- [ ] Decide: should the app run at Windows startup? (deferred from Session 001)
- [ ] Decide: unpin gesture — right-click vs drag-off (deferred from Session 001)

---

## 2026-04-14 | Session 003 — Phase 2 kickoff (tray icon + gtest scaffold)

### Code changes
- New `src/system/TrayIcon.{h,cpp}` — owns a `HWND_MESSAGE` receiver window, registers a `NOTIFYICONDATAW` entry with a custom `WM_APP+1` callback, and shows a right-click popup menu whose single `IDM_QUIT` command calls `PostQuitMessage(0)`. `SetForegroundWindow(m_hwnd)` before `TrackPopupMenu` so the menu dismisses on outside-click (standard Win32 tray quirk). Icon uses `IDI_APPLICATION` as placeholder — replace with a real .ico during Phase 6.
- `main.cpp` creates the tray icon after both windows are up; destructor `Shell_NotifyIcon(NIM_DELETE)` removes the icon cleanly.
- Removed `PostQuitMessage(0)` from `DockWindow::WndProc` and `MenuBarWindow::WndProc` `WM_DESTROY` handlers — tray is now the sole quit path. This retires the tech-debt item called out in Session 002.

### Test scaffolding (DEC-010)
- `tests/CMakeLists.txt` pulls `googletest` v1.14.0 via FetchContent, forces `gtest_force_shared_crt=ON` (needed because the app uses `/MT` static CRT; mismatching CRTs would cause link errors — gtest's default is shared, so we override), calls `gtest_discover_tests` to register with ctest.
- Root `CMakeLists.txt` gates the whole thing behind `option(BUILD_TESTS OFF)` so the default release .exe build does not download ~5MB of gtest sources.
- `tests/smoke_test.cpp` is intentionally trivial (`EXPECT_EQ(2+2, 4)`) — it proves FetchContent, the MSVC linker, and the ctest discovery path all work. Replace once `ConfigManager` lands.

### Build + Smoke Test Instructions (for Bala, Session 003)
From a Visual Studio "Developer Command Prompt for VS 2022":
```
cd "C:\Users\yudhi\OneDrive\Documents\windows to macos converter"
cmake -S . -B build -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release
build\bin\Release\macOSWin.exe
```
Expected: taskbar hides, dock + menu bar appear as before, **and** a notification-area icon now appears (generic Windows app icon). Right-click it → "Quit macOS Overlay" → app exits cleanly, taskbar returns, tray icon disappears. No Task Manager needed.

Then verify test pipeline:
```
ctest --test-dir build -C Release --output-on-failure
```
Expected: `Smoke.PipelineIsAlive` passes.

### Observations / Tech Debt
- Tray icon currently uses `IDI_APPLICATION` — fine for now, but on Windows 11 this shows a rather anonymous generic icon. Phase 6 should replace it with a proper .ico resource embedded in the exe.
- `RegisterClassExW` return value ignored in `TrayIcon::Create` — safe because `CreateWindowExW` will fail if registration failed, and we check that. Documented in code.
- `gtest_force_shared_crt` is set to ON even though the app uses `/MT`. This is intentional: the variable name is misleading — setting it ON actually tells gtest "use whatever CRT model the parent chooses" rather than forcing its own. Our override `MSVC_RUNTIME_LIBRARY=MultiThreaded$<Debug:Debug>` on the test target then matches the app's static CRT. Verify at first `ctest` run.

### Mid-session fixes (in-session)

**Linker errors on `gtest.lib` (`__imp_*` unresolved externals)**
- Root cause: `gtest_force_shared_crt ON` made gtest build with `/MD` (dynamic CRT) while the test target was configured for `/MT` (static CRT). My earlier LOG comment had this inverted — the flag name means what it says: ON = shared, OFF = static.
- Fix: `set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)` in `tests/CMakeLists.txt`. Now both gtest and the test exe use `/MT`, matching the app.
- Rebuild requires a clean `build/` dir so the already-compiled `gtest.lib` is discarded.

**Tray icon never appeared**
- Root cause: `TaskbarManager::Hide()` hides `Shell_TrayWnd`, which *is* the notification area. Tray icon registered fine but the tray it would render into is invisible, and the "^" overflow chevron is also gone. Option A of DEC-009 was silently broken under Phase 1's taskbar-hide behavior.
- Fix: DEC-009 revised to Option C — added a global hotkey **Ctrl+Alt+Q** as primary quit path via `RegisterHotKey(m_hwnd, HOTKEY_QUIT_ID, MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, 'Q')` in `TrayIcon::Create`, handled in `WM_HOTKEY` of the hidden receiver window. Tray icon code left untouched — it becomes usable automatically if the taskbar ever shows again (e.g. after Phase 6 refactor).
- `MOD_NOREPEAT` prevents held-down keys from firing the handler multiple times.

### Second CRT fix — gtest v1.14 ignores `gtest_force_shared_crt`
- After the first CRT fix, link still failed with `'MD_DynamicRelease' doesn't match 'MT_StaticRelease' in smoke_test.obj`. Root cause: googletest v1.14 no longer wires `gtest_force_shared_crt` into its build — the variable is inert.
- Real fix: set `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` in `tests/CMakeLists.txt` **before** `FetchContent_MakeAvailable(googletest)`. Policy CMP0091 NEW (default ≥ CMake 3.15; we require 3.20) propagates it to every gtest target at configure time. Belt-and-suspenders: after gtest's targets exist, explicitly `set_property(TARGET gtest gtest_main gmock gmock_main PROPERTY MSVC_RUNTIME_LIBRARY ...)`.
- Kept the now-inert `gtest_force_shared_crt OFF` for forward compatibility with older gtest versions.

### Verification (2026-04-14, end of Session 003) — GREEN
- **Ctrl+Alt+Q quit:** works. App exits cleanly, taskbar returns.
- **`ctest --test-dir build -C Release --output-on-failure`:** 100% tests passed (`Smoke.PipelineIsAlive`). Test pipeline is live.

### Open items carried forward
- [ ] Run-at-Windows-startup decision (deferred from Session 001). Not blocking Phase 2 component work.
- [ ] Unpin gesture decision — right-click vs drag-off. Will surface during Phase 3.
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Tray icon is registered but not visible while `Shell_TrayWnd` is hidden. Expected behavior until Phase 6 refactors taskbar-hide; no action required now.
- [ ] Run-at-Windows-startup decision (deferred from Session 001).
- [ ] Unpin gesture decision — right-click vs drag-off (deferred from Session 001). Likely surfaces during Phase 3.
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).

---

## 2026-04-15 | Session 004 — Phase 2 component work (ConfigManager tests + Dock population)

### Code changes
- `src/dock/DockWindow.cpp` — Phase 2 behavior landed. `Create()` loads pinned apps via `ConfigManager::Load()` and feeds each through `AddIcon()`. `AddIcon()` uses `SHGetFileInfoW(SHGFI_ICON | SHGFI_LARGEICON)` for icon extraction (handles `.lnk` correctly, unlike `ExtractAssociatedIcon` which returns the shortcut's own icon). `Reposition()` recomputes width from icon count, recenters via `SetWindowPos`, and assigns bounds for each icon. `OnPaint()` renders each icon via `DrawIconEx`. `OnLButtonUp()` hit-tests and delegates to `AppLauncher::LaunchOrFocus()`.
- `tests/config_manager_test.cpp` — 7 gtest cases covering missing/malformed/empty/round-trip/Unicode. Fixture deletes the exe-adjacent `pinned_apps.json` on SetUp/TearDown so order-dependence is impossible.
- `tests/CMakeLists.txt` — test target now also compiles `src/config/ConfigManager.cpp` and includes `vendor/` for `nlohmann/json.hpp`.

### Design notes
- **Icon extraction API choice.** `SHGetFileInfoW` is deliberate. `ExtractAssociatedIcon` is simpler but returns the *shortcut's* icon for `.lnk` files rather than the target app's icon. `SHGetFileInfoW` with `SHGFI_ICON | SHGFI_LARGEICON` resolves the target automatically and returns an `HICON` the caller owns and must `DestroyIcon`. `DockIcon`'s destructor does that cleanup.
- **Empty dock stays visible.** `DOCK_EMPTY_WIDTH = 120` when `m_icons.empty()`. If we collapsed to 0-width the window would disappear and there'd be nothing to drop onto in Phase 3. Cheap and user-observable — they always know the app is running.
- **`AddIcon` calls `Reposition` on every insert.** O(N²) on startup but N is tiny (user's pinned app count). Keeps the code path identical between startup load and Phase 3 drag-drop.
- **Fallback icon.** If `SHGetFileInfoW` fails (broken path, deleted target), we load `IDI_APPLICATION` and `CopyIcon` it so `DockIcon`'s destructor can `DestroyIcon` without clobbering a shared system icon. The tile stays clickable so the user can notice + fix their `pinned_apps.json`.

### Build + Test Instructions (for Bala, Session 004)
From a Visual Studio "Developer Command Prompt for VS 2022":
```
cd "C:\Users\yudhi\OneDrive\Documents\windows to macos converter"
cmake -S . -B build -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Expected ctest output:
- `Smoke.PipelineIsAlive` — PASS (carried from Session 003)
- `ConfigManagerTest.LoadMissingFileReturnsEmpty` — PASS
- `ConfigManagerTest.LoadMissingFileCreatesEmptyConfig` — PASS
- `ConfigManagerTest.SaveLoadRoundTrip` — PASS
- `ConfigManagerTest.LoadMalformedJsonReturnsEmpty` — PASS
- `ConfigManagerTest.LoadEntryWithEmptyPathIsSkipped` — PASS
- `ConfigManagerTest.UnicodeRoundTrip` — PASS
- `ConfigManagerTest.SaveEmptyListProducesEmptyArray` — PASS

Then a manual smoke:
1. Populate `build\bin\Release\pinned_apps.json` (yes, next to the exe — that's where `GetConfigPath()` looks) with something like:
   ```json
   [
     { "name": "Notepad",    "path": "C:\\Windows\\notepad.exe" },
     { "name": "Calculator", "path": "C:\\Windows\\System32\\calc.exe" }
   ]
   ```
2. `build\bin\Release\macOSWin.exe`. Expect: taskbar hides, menu bar at top, dock at bottom with **two icons** (real Notepad + Calc icons, not generic).
3. Click the Notepad icon → Notepad opens. Click again while it's open → Notepad is brought to front.
4. Ctrl+Alt+Q to quit; taskbar returns.

### Observations / Tech Debt
- `pinned_apps.json` lives next to the exe. Release exe ends up at `build\bin\Release\`. That's where the config is expected. For production install we'll likely want `%APPDATA%\macOSWin\` — raise as a decision when we start Phase 6 packaging.
- `CopyIcon` of `LoadIcon(IDI_APPLICATION)` is a small leak safety net — we own the copy, destructor destroys it. If `LoadIconW` returns null (can't happen for IDI_APPLICATION on Windows) we'd pass null to the ctor and the tile would render blank; acceptable.
- No visual state for "icon failed to load" vs "icon is generic fallback." Phase 6 polish.
- DockWindow `/W4` may warn on the unused `x`, `y` in `OnMouseMove`, `OnMouseLeave` — parameters are named in the header but I silenced them in the .cpp with `/*x*/`, `/*y*/`. Keep an eye on warnings in Bala's build log.

### Mid-session fixes (end of Session 004)

**`LoadIconW` LPSTR/LPCWSTR compile error**
- `IDI_APPLICATION` expands to an `LPSTR` resource pointer (via `MAKEINTRESOURCE`). `LoadIconW` (the explicit W variant) wants `LPCWSTR`. The macro `LoadIcon` would have resolved to `LoadIconW`+wide resource under `UNICODE`, but calling `LoadIconW` directly doesn't get that translation.
- Fix in `DockWindow::AddIcon`: `LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION))`.

**`explorer.exe` click did nothing — false-positive focus-on-desktop**
- `AppLauncher::LaunchOrFocus` enumerates top-level windows looking for one owned by a process with the matching exe name. For `explorer.exe` that matches *many* windows beyond File Explorer — `Progman` (desktop), `WorkerW` (wallpaper worker), `Shell_TrayWnd` (taskbar), `NotifyIconOverflowWindow`, etc. The enumeration hit the desktop (`Progman`) first, `ForceForeground`'d it, and returned success. From the user's perspective nothing happened because they were already looking at the desktop.
- Fix: added `IsUserAppWindow(HWND)` filter in `AppLauncher.cpp`. Requires `IsWindowVisible`, unowned (`GW_OWNER == null`), no `WS_EX_TOOLWINDOW`, non-empty `GetWindowTextLength`, and class name not in `{Progman, WorkerW, Shell_TrayWnd, Shell_SecondaryTrayWnd, NotifyIconOverflowWindow, TrayNotifyWnd}`. This is effectively the same heuristic the alt-tab implementation uses for "real app window."
- Verified: first click opens a new File Explorer window, subsequent click with one open brings it to front.

### Verification (2026-04-15, end of Session 004) — GREEN
- Build clean (MSVC 18.4.0 via Developer PowerShell), rebuilt exe dated 2026-04-15.
- Manual smoke: dock loads 2 icons from `pinned_apps.json`, real icons rendered (not generic fallback), clicking Claude launches Claude, clicking File Explorer opens File Explorer, Ctrl+Alt+Q quits cleanly.
- `ctest` run deferred to Session 005 — test code compiles and links, pass verification is bookkeeping.

### Open items carried forward
- [ ] Session 005: run `ctest` to formally mark the 7 ConfigManager tests green in TESTING.md.
- [ ] Session 005: tag v0.2.0 in CHANGELOG, raise DEC-011 (drag-to-pin semantics) + DEC-012 (unpin gesture).
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).
- [ ] Consider: should clicking Explorer always open a new window (macOS Finder-like), rather than focusing an existing window? Arguable UX question — defer.

---

## 2026-04-16 | Session 005 — Phase 2 close-out + DEC-013 AppBar fix

### Bookkeeping
- All 8 automated tests verified green by Bala (`ctest` run). Results recorded in `docs/TESTING.md` rows A-001..A-008 + Run #2 row.
- v0.2.0 already tagged in CHANGELOG by Session 004. No change needed.
- DEC-011 (drag-to-pin semantics: `.exe`+`.lnk` only / flash-red reject / silent duplicate ignore / silent multi-skip) — APPROVED by Bala.
- DEC-012 (unpin gesture: drag-off-dock) — APPROVED by Bala.
- DEC-013 (reserve screen work area via AppBar API) — APPROVED by Bala (Option A + sub-choice A1: full-width strip).

### Code changes
- New `src/system/AppBarManager.{h,cpp}` — RAII wrapper around `SHAppBarMessage`. Creates an invisible fully-transparent helper HWND per edge. Registers with `ABM_NEW`, claims a strip via `ABM_QUERYPOS`/`ABM_SETPOS`, handles `ABN_POSCHANGED` for display changes. Destructor calls `ABM_REMOVE`. Uses `WM_APP+10` callback message.
- `src/main.cpp` — creates two `AppBarManager` instances before the visible windows: one for `ABE_TOP` (28px, MenuBar height), one for `ABE_BOTTOM` (84px, Dock height + bottom gap). Registration is non-fatal — if it fails the overlay still runs. Both are RAII-destructed on exit.
- `CMakeLists.txt` — added `src/system/AppBarManager.cpp` to source list.
- `CLAUDE.md` — added `AppBarManager.h/.cpp` to file reference map.

### Design notes
- **Why a separate invisible HWND instead of registering DockWindow/MenuBarWindow directly?** The Dock is a centered pill of dynamic width but the AppBar reservation must be a full-width strip (DEC-013 A1). Registering the Dock HWND as an AppBar would force it to be full-width, breaking its visual layout. A separate invisible full-width helper keeps the visible Dock code untouched. The MenuBar IS full-width so could theoretically self-register, but using the same pattern for both keeps the code uniform and avoids adding ABN_ handling to MenuBarWindow's WndProc.
- **Helper HWND at `HWND_BOTTOM` z-order.** The invisible AppBar windows are positioned below everything — they're never seen. The visible Dock/MenuBar windows remain `HWND_TOPMOST` independently.
- **Thickness constants duplicated in main.cpp.** `MENUBAR_HEIGHT=28` and `DOCK_TOTAL_HEIGHT=84` are copies of the window classes' `BAR_HEIGHT` and `DOCK_HEIGHT+BOTTOM_GAP`. If those change, main.cpp's constants must be updated too. Acceptable for now; if it becomes a maintenance pain, extract shared constants to a `LayoutConstants.h`.

### Mid-session fix — AppBar API caused half-screen maximize

**Problem:** After implementing DEC-013 with `SHAppBarMessage` (AppBar API), maximized windows only filled ~50% of the screen. The hidden taskbar (`Shell_TrayWnd`) retains its own AppBar registration internally even when `ShowWindow(SW_HIDE)` hides it. Our bottom AppBar stacked on top of the taskbar's reservation, causing Windows to over-shrink the usable work area.

**Fix:** Rewrote `AppBarManager` to use `SystemParametersInfo(SPI_SETWORKAREA)` instead. This directly sets the work area rect to `{0, 28, screenW, screenH-84}`, bypassing the stacking problem entirely. RAII pattern: constructor saves original work area, destructor restores it. No helper HWNDs, no `SHAppBarMessage`, no `ABN_*` callbacks — just one clean rect set.

**Trade-off vs. AppBar approach:** If our process is killed hard (power loss, `taskkill /F`), the shrunk work area persists until the user shows/hides the Windows taskbar (which resets it) or reboots. The AppBar API would have cleaned up automatically via Explorer. Acceptable trade-off because: (a) Ctrl+Alt+Q clean quit is the normal path, (b) the RAII guard handles all code-path exits, (c) worst case is a slightly smaller maximizable area that's trivially fixed.

**Files changed:** `AppBarManager.h` (complete rewrite — takes `(hInstance, topPx, bottomPx)`, owns a sentinel HWND), `AppBarManager.cpp` (rewrite), `main.cpp` (simplified to one `workArea.Apply()` call).

**DEC-013 revision:** Original decision specified Option A (SHAppBarMessage). Revised to SPI_SETWORKAREA with RAII + sentinel enforcement due to the stacking conflict with the hidden taskbar. Functionally identical from the user's perspective — maximized windows respect our bars.

### Second mid-session fix — Explorer still overwriting work area

**Problem (attempt 2):** First SPI_SETWORKAREA rewrite still failed. `SPIF_SENDCHANGE` broadcasts `WM_SETTINGCHANGE` to all top-level windows; Explorer receives it, sees Shell_TrayWnd is hidden, and resets the work area to full-screen — overwriting our setting. Even with a sentinel watching `WM_SETTINGCHANGE`, the broadcast creates a fight loop between our sentinel and Explorer.

**Fix:** Set work area **silently** with `fWinIni = 0` (no broadcast). Explorer never hears about it, so it never fights back. Added a 500ms timer in the sentinel as belt-and-suspenders — periodically checks `SPI_GETWORKAREA` and silently re-applies if anything overwrites it. Sentinel window created BEFORE the initial `SPI_SETWORKAREA` call so it's ready to catch async resets immediately. Destructor restores original work area WITH `SPIF_SENDCHANGE` so apps return to normal on exit.

**Verified by Bala:** maximized windows now stop at menu bar (top) and dock (bottom). Ctrl+Alt+Q restores cleanly.

### Open items carried forward
- [x] Session 005: run `ctest` — DONE, 8/8 PASS.
- [x] Session 005: raise DEC-011 + DEC-012 — DONE, both APPROVED.
- [x] DEC-013 AppBar implementation — DONE, pending Bala's build + smoke test.
- [ ] Phase 3 implementation (DockDropHandler + ProcessMonitor) — blocked on DEC-013 verification.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).
- [ ] Consider: should clicking Explorer always open a new window (macOS Finder-like)? Defer.
- [ ] Thickness constants duplicated between window classes and main.cpp. Low-priority tech debt.

---

## 2026-04-16 | Session 006 — Phase 3 Implementation

### Code changes
- **`DockDropValidator.{h,cpp}`** (new) — pure-logic file-type validation (`IsValidAppFile`) and display-name extraction (`ExtractAppName`). Separated from `DockDropHandler` so these can be unit-tested without Win32 UI dependencies.
- **`DockDropHandler.{h,cpp}`** — rewritten. `HandleDrop` iterates dropped files via `DragQueryFileW`, validates with `DockDropValidator::IsValidAppFile`, checks duplicates via `DockWindow::HasIcon`, delegates pinning to `DockWindow::AddIcon`. Returns count of pinned files.
- **`DockWindow.{h,cpp}`** — Phase 3 behaviors:
  - `HasIcon(path)` — case-insensitive duplicate detection for DEC-011 sub-3.
  - `SaveConfig()` — persists current icon list to `pinned_apps.json` via `ConfigManager::Save`.
  - `FlashReject()` — 200ms red background flash on rejected drop (DEC-011 sub-2).
  - `OnDropFiles` — delegates to `DockDropHandler::HandleDrop`, saves config on success, flashes red if all files rejected.
  - `OnLButtonDown` — captures mouse on icon click, records drag start position.
  - `OnMouseMove` — transitions click to drag once 8px threshold exceeded; hides icon during drag.
  - `OnLButtonUp` — if dragging and mouse outside dock bounds, removes icon + saves config (DEC-012). If not dragging, launches/focuses app.
  - `OnTimer(WPARAM)` — dispatches by timer ID. `TIMER_PROCESS_MONITOR` (1500ms): polls `ProcessMonitor::GetRunningAppNames()`, updates each icon's running state, repaints on change. `TIMER_FLASH_REJECT`: ends flash-red period.
  - `OnPaint` — draws white indicator dots below running icons; skips rendering dragged icon.
- **`tests/drop_handler_test.cpp`** (new) — 17 gtest cases for `DockDropValidator`: 11 for `IsValidAppFile` (accepts .exe/.lnk, case-insensitive; rejects .txt/.bat/.cmd/.url/no-ext/empty/directory), 6 for `ExtractAppName` (path parsing, spaces, Unicode, bare names).
- **`tests/CMakeLists.txt`** — added `drop_handler_test.cpp` + `DockDropValidator.cpp` to test target; added `src/` to include path.
- **`CMakeLists.txt`** — added `DockDropValidator.cpp` to main source list.
- **`CLAUDE.md`** — added `DockDropValidator.h/.cpp` to file reference map.

### Design notes
- **Validator split.** `DockDropHandler::HandleDrop` depends on `DockWindow*` (for `HasIcon`/`AddIcon`). Compiling it in the test binary would require the entire Dock + Win32 dependency chain. Extracting the pure-logic functions into `DockDropValidator` keeps the test binary lightweight.
- **Drag-off threshold.** 8px Euclidean distance prevents accidental unpin on sloppy clicks. During a drag, the icon disappears from paint (visually "lifted"). If the user releases back inside the dock, it reappears — no data loss.
- **Process monitor for .lnk files.** The exe-name matching won't find running apps for `.lnk` shortcuts because the shortcut filename (e.g. `Chrome.lnk`) doesn't match the process name (e.g. `chrome.exe`). Resolving shortcut targets requires `IShellLink` COM calls — deferred to a future enhancement. Direct `.exe` pins work correctly.
- **Flash-red on partial multi-drop.** Current logic: flash only fires if ALL dropped files were rejected (pinned == 0). If some files pin and some are skipped (wrong type or duplicate), no flash — the successful pins are visual confirmation enough.

### Build + Test Instructions (for Bala, Session 006)
From a Visual Studio "Developer Command Prompt for VS 2022":
```
cd "C:\Users\yudhi\OneDrive\Documents\windows to macos converter"
rmdir /s /q build
cmake -S . -B build -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Expected ctest output (25 tests total):
- `Smoke.PipelineIsAlive` — PASS
- `ConfigManagerTest.*` (7 tests) — PASS
- `DropValidator.*` (17 tests) — PASS

Manual smoke test:
1. `build\bin\Release\macOSWin.exe` — taskbar hides, dock + menu bar appear.
2. Drag a `.exe` file (e.g. `C:\Windows\notepad.exe`) onto the dock — icon appears.
3. Drag the same `.exe` again — nothing happens (duplicate ignored).
4. Drag a `.txt` file onto the dock — dock flashes red briefly, no icon added.
5. Click the pinned Notepad icon — Notepad opens. A white dot should appear under the icon within ~1.5s.
6. Close Notepad — the dot disappears within ~1.5s.
7. Click and drag the Notepad icon off the dock (move mouse above/below) and release — icon disappears, unpinned.
8. Ctrl+Alt+Q to quit; taskbar returns.

### Open items carried forward
- [x] Phase 3 implementation — DONE (pending Bala's build + smoke test).
- [ ] .lnk running-indicator resolution — requires IShellLink COM to resolve shortcut target. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).
- [ ] Consider: should clicking Explorer always open a new window (macOS Finder-like)? Defer.
- [ ] Thickness constants duplicated between window classes and main.cpp. Low-priority tech debt.

---

## 2026-04-17 | Post-Session 006 — Known Issues Reported by Bala

### Issues to fix next session

1. **Work area not applied immediately on launch** — on first launch, app windows (e.g. a maximised browser) extend behind/above the menu bar. After the user minimises and restores, the work area snaps into place correctly. Root cause likely: `AppBarManager`'s sentinel timer hasn't fired yet, or the `SPI_SETWORKAREA` call isn't beating Explorer's initial layout pass. Fix: investigate calling `Apply()` earlier (before the message loop) or adding a one-shot 0ms timer to re-apply after the first message pump cycle.

2. **Settings (UWP) icon looks wrong** — the icon extracted for the Settings app via `IShellItemImageFactory` doesn't match the real Windows Settings gear icon. May be extracting a low-res or wrong-size bitmap. Fix: try requesting a larger size (64×64 or 256×256) from `IShellItemImageFactory::GetImage`, or try `SIIGBF_ICONONLY` flag.

3. **Dock icons are too large and pixelated** — current `ICON_SIZE` constant renders icons at a size that looks blurry/scaled. Fix: audit `ICON_SIZE` constant in `DockWindow.h` and compare against typical macOS dock icon sizes (48px logical, high-DPI aware). May need `DrawIconEx` with correct destination rect, or switch to Direct2D bitmap scaling with `D2D1_BITMAP_INTERPOLATION_MODE_LINEAR` for Phase 4 groundwork. Address in next session before starting Phase 4.

### Open items carried forward
- [x] Work area immediate-apply fix — resolved in Session 007 (see below)
- [x] Settings icon quality — resolved in Session 007
- [x] Dock icon size + pixelation — resolved in Session 007
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).

---

## 2026-04-17 | Session 007 (round 2) — Root-cause analysis and revised fixes

### Why Session 007 round 1 fixes had no effect

All three first-attempt fixes shared the same core misdiagnosis:

1. **Work area (round 1):** Used `PostMessage(WM_SETTINGCHANGE)` to each maximized window. Modern apps (Chrome, Edge, WinUI) ignore direct `WM_SETTINGCHANGE` messages — they only re-snap on a system broadcast, which we can't use safely (triggers Explorer fight-back). Fix: `SetWindowPos` to directly resize each maximized window to the work area bounds.

2. **Icons same size (root cause):** `IShellItemImageFactory::GetImage({48,48})` at small sizes internally upscales from the system cache (32×32), producing a blurry 48×48 bitmap — same problem one layer deeper. Fix: request 256×256 from the factory, then let `DrawIconEx` downscale.

3. **Icons still blurry (root cause):** `DrawIconEx` uses GDI's current stretch mode. Default is `COLORONCOLOR` (nearest-neighbour), which is blocky. Fix: `SetStretchBltMode(hdc, HALFTONE)` + `SetBrushOrgEx` before the draw loop — gives bilinear-quality downscaling from the 256×256 source.

4. **UWP icon (round 1):** Changed `SIIGBF_ICONONLY` → `SIIGBF_RESIZETOFIT` (= 0x0). That returns a shell thumbnail/screenshot, not an icon. Fix: keep `SIIGBF_ICONONLY`, just use 256×256 size.

### Revised fixes (round 2)

- `AppBarManager.cpp`: `NotifyMaximizedProc` → `ForceSnapMaximizedProc` using `SetWindowPos`. Same call added in `EnforceWorkArea()` so Explorer fight-back re-snaps windows too.
- `DockWindow.cpp ExtractUwpIcon`: Reverted to `SIIGBF_ICONONLY`, kept 256×256 size.
- `DockWindow.cpp AddIcon`: Factory request changed to `{256,256}` with `SIIGBF_ICONONLY`. Mask updated to 256×256.
- `DockWindow.cpp OnPaint`: `SetStretchBltMode(hdc, HALFTONE)` + `SetBrushOrgEx` added before the DrawIconEx loop.

### Open items carried forward
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).

---

## 2026-04-17 | Session 007 — Post-Phase-3 bug fixes

### Fix 1: Work area not applied to already-maximized windows

**Root cause:** `SPI_SETWORKAREA` with `fWinIni = 0` (our silent approach) sets the system work area correctly but posts no `WM_SETTINGCHANGE` broadcast. Windows already maximized *before* our app launched never receive the notification and thus never recompute their size. Only newly-maximized windows (opened after our call) used the updated rect.

**Fix:** After the silent `SPI_SETWORKAREA` in `AppBarManager::Apply()`, call `EnumWindows(NotifyMaximizedProc, 0)`. The callback checks `IsWindowVisible && IsZoomed`, skips Explorer shell windows (`Shell_TrayWnd`, `Progman`, `WorkerW`, `Shell_SecondaryTrayWnd`, `NotifyIconOverflowWindow`) to avoid triggering the fight-back reset, and `PostMessage`s `WM_SETTINGCHANGE(SPI_SETWORKAREA, 0)` to each remaining maximized window. Those windows then snap immediately.

**File changed:** `src/system/AppBarManager.cpp`

### Fix 2: Settings (UWP) icon looks wrong

**Root cause:** `ExtractUwpIcon` was requesting a 48×48 bitmap with `SIIGBF_ICONONLY`. `SIIGBF_ICONONLY` strips the tile's background, leaving only the transparent glyph layer — for Settings (and most UWP apps), the artwork IS the coloured tile, so stripping the background produces an incorrect/incomplete icon. Additionally, 48×48 is too small for quality.

**Fix:**
- `SIZE sz` changed from `{48, 48}` to `{256, 256}` — ensures high-res source for downscaling.
- `SIIGBF_ICONONLY` replaced with `SIIGBF_RESIZETOFIT` — preserves tile background + icon artwork.
- Mask size updated to match: `CreateBitmap(256, 256, ...)`.

**File changed:** `src/dock/DockWindow.cpp` (`ExtractUwpIcon`)

### Fix 3: Dock icons too large and pixelated

**Root cause:** `ICON_SIZE = 56` caused two problems:
1. `SHGetFileInfoW(SHGFI_LARGEICON)` returns a 32×32 `HICON`. `DrawIconEx` stretches it to 56×56 → blurry upscale.
2. 56px is larger than typical macOS default dock icons (48px logical at 1x DPI).

**Fix:**
- `ICON_SIZE` reduced 56→48 px in `DockWindow.h`.
- `AddIcon` regular path: before the `SHGetFileInfoW` fallback, try `IShellItemImageFactory::GetImage({48, 48}, SIIGBF_ICONONLY|SIIGBF_RESIZETOFIT)`. The factory returns a bitmap at exactly 48×48, so `DrawIconEx` renders at native size — no upscaling, no blur. `SHGetFileInfoW(SHGFI_LARGEICON)` kept as fallback if the factory fails.

**Files changed:** `src/dock/DockWindow.h`, `src/dock/DockWindow.cpp` (`AddIcon`)

### Open items carried forward
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6 polish).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).
- [ ] Consider: should clicking Explorer always open a new window (macOS Finder-like)? Defer.
- [ ] Thickness constants duplicated between window classes and main.cpp. Low-priority tech debt.

---

## 2026-04-17 | Session 008 — Dock visual polish (GDI+ rendering, Settings icon, sizing, transparency)

### Root causes identified during this session
- **`DrawIconEx` ignores `SetStretchBltMode`** when scaling an HICON. HALFTONE had zero effect — tested and confirmed. Use GDI+ for quality scaling.
- **`AlphaBlend` is nearest-neighbour**, not bilinear. It scales correctly but without filtering. Also produced blocky output.
- **`Gdiplus::Bitmap::FromHBITMAP` drops the alpha channel** — renders with black/pink fringe on anti-aliased icon edges. `FromHICON` preserves alpha. Fix: wrap factory HBITMAP into an HICON (`CreateIconIndirect`) before rendering.
- **`SIIGBF_ICONONLY` returns blank for Windows 11 Settings**. The package's "icon" resource is a monochrome glyph that the factory can't expose at 256×256. The factory's tile-logo path (no ICONONLY flag) correctly returns the colored tile.
- **`SHIL_JUMBO` imagelist** gives the real 256×256 artwork for *regular* apps (exe/lnk) but often returns a generic glyph for UWP apps. Correct ordering: JUMBO for .exe/.lnk, factory-tile first for UWP.

### Final rendering pipeline
1. `AddIcon` obtains an HICON (preferred) or HBITMAP from whichever source works.
2. `OnPaint` wraps each source in `Gdiplus::Bitmap` via `FromHICON` / `FromHBITMAP`.
3. `Graphics::DrawImage` with `InterpolationModeHighQualityBicubic` + `PixelOffsetModeHighQuality` renders to the dock DC at `ICON_SIZE`.

### Dock geometry settled on
- `ICON_SIZE = 28`, `ICON_PADDING = 20`, `DOCK_HEIGHT = 40`, `BOTTOM_GAP = 2`.
- `main.cpp` `DOCK_TOTAL_HEIGHT` reserve = 42 (= 40 + 2).
- Chroma-key transparency via `LWA_COLORKEY(RGB 255,0,255)`; magenta fill in `OnPaint`.

### Carryover / deferred
- [ ] Consolidate `DockIcon` HICON+HBITMAP fields into a single render source.
- [ ] Replace chroma-key with DirectComposition acrylic (Phase 6).

---

## 2026-04-17 | Session 009 — Phase 4 Animation (fish-eye magnification)

### Decisions approved
- DEC-014: 16ms `WM_TIMER` (Option A)
- DEC-015: Keep GDI+ for animated scaling (Option A)
- DEC-016: Cosine falloff curve (Option B)

### Implementation notes
- `DOCK_WINDOW_HEIGHT=72` gives the dock window 32px of transparent headroom above the visible icon strip. Max magnified icon = `28 * 1.8 = 50px`; top of that icon in client coords = `66 - 50 = 16px` — well within the 72px window. No clipping.
- Cosine formula per icon: `scale = 1 + 0.8 × 0.5 × (1 + cos(π × dist/120))` where `dist` is the horizontal distance from cursor to icon slot center.
- Draw order sorted by ascending scale so the most-magnified icon (closest to cursor) renders on top of its neighbors. This prevents a larger icon from being occluded by adjacent smaller icons.
- `DOCK_TOTAL_HEIGHT=42` in `main.cpp` (work-area reservation) is unchanged — only the window height increased.
- `TrackMouseEvent` must be re-armed after every `WM_MOUSELEAVE` — `m_mouseTracking` flag tracks this. Timer starts on first `WM_MOUSEMOVE` into the window, stops on `WM_MOUSELEAVE`.

### Phase 4 bugs fixed this session (post-implementation)

1. **Oscillation / icons going big-small repeatedly** — Root cause: `LWA_COLORKEY` transparent pixels pass mouse events through; cursor crossing a gap between icons triggered `WM_MOUSELEAVE`, which reset scale, which shrank icons, which put cursor back over icon pixels, repeating. Fix: moved cursor-outside check into `TIMER_ANIMATE` — calls `GetCursorPos` + `PtInRect` every 16ms as the authoritative reset trigger. `WM_MOUSELEAVE` now only clears `m_mouseTracking`.
2. **Frame flicker** — Root cause: two-step `OnPaint` (fill magenta, then draw icons) had a visible gap between erase and redraw. Fix: double-buffered `OnPaint` — all drawing to off-screen memory DC, committed atomically with `BitBlt`. Also added `WM_ERASEBKGND → return 1` to suppress the OS erase step.
3. **Slow reaction time** — Root cause: cursor position updated on `WM_MOUSEMOVE` but repaint waited for the next 16ms timer tick. Fix: `InvalidateRect(FALSE)` called directly in `OnMouseMove` for immediate response.

### Open items carried forward
- [ ] Phase 5 planning (menu bar: clock, battery, volume, Wi-Fi, active app name).
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6).
- [ ] Config location (`%APPDATA%` vs next-to-exe) for installed builds (Phase 6).
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Replace `IDI_APPLICATION` tray icon with a real .ico (Phase 6).
- [ ] Decide: config location (next-to-exe vs `%APPDATA%`) for installed builds (Phase 6).

---

## 2026-04-17 | Session 010 — Phase 5 Implementation (Menu Bar)

### Decisions approved
- DEC-017 (clock format macOS-style), DEC-018 (all 4 widgets v1), DEC-019 (static widgets), DEC-020 (FileDescription name), DEC-021 (vector Apple glyph v1). Bala approved all with a single "choose best for mac os look" directive.

### Implementation notes
- **Version info extraction.** `GetFileVersionInfoSizeW` → `GetFileVersionInfoW` → `VerQueryValueW` chain. Translation table (`\VarFileInfo\Translation`) gives lang+cp; first entry used to build the `\StringFileInfo\<lang><cp>\FileDescription` sub-block path. Cache on exe path — version info is immutable per exe install.
- **Shell-window filter.** `Progman`, `WorkerW`, `Shell_TrayWnd`, `Shell_SecondaryTrayWnd`, `NotifyIconOverflowWindow`, `TrayNotifyWnd`, and our own `macOSWin_MenuBar` / `macOSWin_Dock` / `macOSWin_TrayReceiver` classes are blacklisted from firing `SetActiveAppName`. Without this, alt-tabbing to the desktop shows "Progman" in the menu bar.
- **Volume via WASAPI.** `IMMDeviceEnumerator::GetDefaultAudioEndpoint(eRender, eConsole)` → `Activate(IAudioEndpointVolume)` → `GetMasterVolumeLevelScalar`. Relies on OLE apartment already init'd by `OleApartment` in `main.cpp`.
- **Wi-Fi.** `WlanOpenHandle(clientVersion=2)` for Vista+ API. `wlan_intf_opcode_current_connection` returns `WLAN_CONNECTION_ATTRIBUTES` which holds the SSID (UTF-8 byte array, not wide — needs MultiByteToWideChar) and signal quality (0–100).
- **Glyph rendering.** GDI+ `Graphics` on the memDC. Wi-Fi = 3 concentric arcs 210°–330° (top arc = outer, stronger signal = more filled). Volume = speaker rect + cone polygon + 0–3 sound-wave arcs. Battery = rounded rect outline (`GraphicsPath` of four corner arcs closed) + fill rect + optional charging bolt polygon. All with `SmoothingModeAntiAlias`.
- **Apple glyph v1.** Two overlapping filled ellipses = body, a small ellipse = leaf, an inset bg-coloured ellipse = "bite". Not photorealistic, but recognizable and avoids shipping a PNG asset. Phase 6 will swap in `assets/icons/apple.png`.
- **2s refresh timer.** Single `WM_TIMER` at 2000ms refreshes the entire menu bar (repaint invalidates whole client). Clock ticks the same way — minor "once per 2s" jitter on minute boundaries is invisible in practice. 30s separate timer deemed unnecessary given the paint is cheap.
- **Double buffered paint.** Same technique as `DockWindow::OnPaint` post-Session 009 fix. `WM_ERASEBKGND → return 1`; all drawing to off-screen memDC; single `BitBlt` commit.

### Open tech debt
- `SystemInfoBar::Render` computes x positions in GDI+ units but the caller passes a GDI `HDC` — GDI+ writes through that DC fine, but mixing GDI+ and raw GDI into the same buffer (`MenuBarWindow::OnPaint`) is a minor smell. Works because the memDC is 32bpp and GDI+ handles it cleanly.
- The Apple-glyph "bite" is a bg-colour overlay, not a true path subtraction. If Phase 6 acrylic/blur gives the menu bar a non-uniform background, the bite will look wrong and must become a real `GraphicsPath::Subtract` or a clipped region.
- No hover/click feedback on the Apple glyph or widgets. That's DEC-019/021 scope — revisit in Phase 6.

### Open items carried forward
- [ ] Phase 5 build + smoke verification (Bala).
- [ ] Phase 6 planning: acrylic/blur, entrance animation, real Apple asset, optional flyouts.
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).
- [ ] Tray icon placeholder → real .ico (Phase 6).
- [ ] Config location (`%APPDATA%` vs next-to-exe) for installed builds (Phase 6).

---

## 2026-04-18 | Session 012 addendum — Phase 6 experiments reverted

### Key findings (the hard way)
- **Win11 has no real blur path for `WS_EX_LAYERED` windows.** Both `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` and `DwmEnableBlurBehindWindow` reserve a backdrop region but apply no actual blur — leaving either a dark tint (first API) or a flat white fill (second API). Confirmed empirically on Bala's Win11. Any Phase-6 blur work must drop layered-window rendering and adopt DirectComposition with `DWMSBT_TRANSIENTWINDOW` (requires `WS_EX_NOREDIRECTIONBITMAP`).
- **`UpdateLayeredWindow`-based dock slide-up animation unreliable.** Three approaches failed: WM_TIMER + InvalidateRect (timer coalescing + paint-race), WM_TIMER + RedrawWindow(RDW_UPDATENOW), synchronous Sleep-loop + cached bitmap. The sync version's worst failure: overlay stayed invisible entirely (taskbar hidden, no bars — user sees only wallpaper, has to restart Windows).
- **`SetWindowLongPtr(WS_EX_LAYERED)` on foreign windows at `EVENT_OBJECT_SHOW` time is unreliable.** WindowAnimator prototype never triggered a visible animation. Likely UIPI and/or the window's render target being established before we can intercept. Dropped.

### Code lessons
- Always keep a "boring but visible" fallback for rendering. The MenuBarWindow UpdateLayeredWindow rewrite for blur left the bar invisible when blur failed; reverted to BitBlt + opaque `LWA_ALPHA(255)` which is the Phase 5 baseline.
- Synchronous animation loops in `Show()` called before the main message loop starts are especially dangerous — if they fail to render, the user has no tray icon to quit from and no taskbar.
- `EVENT_OBJECT_SHOW` fires frequently; any handler must filter aggressively or tank system perf.

### Deferred decisions
- [ ] DEC-022 real blur — needs DirectComposition rewrite (scoped: 2 sessions).
- [ ] DEC-023 dock slide-up — coupled with above rewrite; easier once rendering is DComp-based.
- [ ] DEC-027 (to be logged): DirectComposition migration plan — pick between (a) two separate DComp visual trees for bars or (b) a single shared DComp device with shared swap chains.

---

## 2026-04-18 | Session 012 — Phase 5 close-out + Phase 6 core polish (original intent)

### Phase 5 formal close
- `CHANGELOG.md` Unreleased → `[0.5.0]`. Phase 5 row in CLAUDE.md flipped to Complete. `TESTING.md` Phase 5 rows filled + Test Run Log appended.

### DEC-022 Blur — design notes
- Chose undocumented `SetWindowCompositionAttribute` over DirectComposition rewrite because it's a single function call per window and is what TranslucentTB/Seelen use in production. Zero impact on rendering pipeline — DWM does the work behind the scene.
- ABGR tint default `0x60181820` — alpha 0x60 (~38%), BGR `0x181820` (near-black with faint blue tint). Matches macOS dark-mode menu bar colour closely.
- Crucial interaction with `UpdateLayeredWindow`: blur composes the *desktop* behind our window; our 32bpp DIB then composites on top with per-pixel alpha. If we used `SetLayeredWindowAttributes(LWA_ALPHA, 255)` the whole DIB would be opaque and blur would be invisible. Hence the MenuBarWindow::OnPaint rewrite this session.

### DEC-023 Entrance — easing math
- Offset formula: `offset = (DOCK_WINDOW_HEIGHT + 20) × (1 - ease)` where `ease = 1 - (1-t)²` (ease-out quadratic).
- At t=0 offset ≈ 92px below target; at t=1 offset = 0.
- 16 frames × 16ms = 256ms. Matches macOS Dock visually (Mission Control shows macOS Dock appears in ~250ms).
- `Reposition()` split-path: during entrance we must not overwrite Y with target Y (would cancel the animation). Hence `if (m_entranceActive) SWP_NOMOVE`.

### DEC-024 Now-Playing — threading
- Reused existing SMTC MTA worker thread. Added `TryGetMediaPropertiesAsync().get()` after play-state fetch. `.get()` is blocking but acceptable on a 1-second poll interval.
- Single mutex (`m_nowPlayingMutex`) protects only the `std::wstring` copy. UI reads under lock (quick copy), worker writes under lock. No contention observed in practice.
- Fallback: if no session or properties fetch throws, set empty string (not last-known). Keeps UI honest when nothing is playing.

### DEC-025 Multi-monitor
- `GetSystemMetrics(SM_CXSCREEN/CYSCREEN)` already returns primary monitor dimensions, so this is primarily a clarity/consistency change. But it's also a hook for future "dock/menu bar on any monitor" extensibility — all call sites now go through `Composition::GetPrimaryMonitorSize()`.
- AppBarManager: work-area reservation via `SPI_SETWORKAREA` is system-wide but only meaningfully applies to the primary monitor anyway. Change is cosmetic here.

### Risks / watch items
- Undocumented API: `SetWindowCompositionAttribute` could be removed in a future Windows build. Mitigation: `GetProcAddress` return is checked, ApplyBlurBehind returns false gracefully — windows will still render (just without blur).
- `UpdateLayeredWindow` requires rebuilding the whole DIB every paint. Menu bar is 38px × screen-width, refresh at 2s intervals. Negligible CPU.
- Slide-up animation starts only on `Show()`. If the dock is already visible and repositioned (e.g. display change), no animation. Acceptable.

### Open items carried forward
- [ ] Phase 6 build + smoke verification (Bala).
- [ ] Phase 6 remaining scope: display-change robustness, crash recovery test, tray icon asset, config location.
- [ ] .lnk running-indicator resolution — IShellLink COM. Low priority.
- [ ] Run-at-Windows-startup decision (still deferred).

---

*Template for future entries:*
```
## YYYY-MM-DD | Session XXX — [Topic]

### [Heading]
- Note
```
