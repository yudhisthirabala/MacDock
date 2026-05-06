# Changelog — macOS Windows Overlay

> Follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format.
> Versions are tagged after each phase is completed and verified.

---

## [Unreleased] — v0.7.0
> Phase 6 — DirectComposition rewrite (DEC-027)

### Added
- `src/system/CompositionHelper.h` — extended with DComp device infrastructure:
  - `DCompWindow` struct (C++17 inline statics): shared D3D11 + DXGI + `IDCompositionDevice`, per-window `IDCompositionTarget` + `IDCompositionVisual` + `IDCompositionSurface` (GDI interop via `IDXGISurface1::GetDC`).
  - `ApplySystemBackdrop(HWND)` — calls `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE, DWMSBT_TRANSIENTWINDOW)` for real OS-level acrylic on Win11 22H2+; fails silently on older builds.
  - `DWMWA_SYSTEMBACKDROP_TYPE` / `DWM_SYSTEMBACKDROP_TYPE` enum — guards for older SDK headers.
- `CMakeLists.txt` — added `d3d11`, `dxgi` link targets.

### Changed
- `DockWindow` (DEC-027): Removed `WS_EX_LAYERED` + `UpdateLayeredWindow` rendering path. Now uses `WS_EX_NOREDIRECTIONBITMAP` + `DWMSBT_TRANSIENTWINDOW` + `DCompWindow` (GDI+ → `IDCompositionSurface`). `WM_NCHITTEST` returns `HTTRANSPARENT` for the headroom zone above the pill. `DOCK_WINDOW_HEIGHT` reduced 72→60 to minimise the acrylic-backdrop zone above the pill.
- `DockWindow::Show` (DEC-023): Entrance slide-up animation re-enabled. Uses `WM_TIMER` at 16ms with `SetWindowPos` only (no per-frame GDI re-render) — reliable because the DComp surface is rendered once and the compositor handles compositing each frame.
- `MenuBarWindow` (DEC-027): Removed `WS_EX_LAYERED` + `SetLayeredWindowAttributes` + `BitBlt` rendering path. Now uses `WS_EX_NOREDIRECTIONBITMAP` + `DWMSBT_TRANSIENTWINDOW` + `DCompWindow`. Bar colours changed from alpha=255 (opaque) to 160–200 (semi-transparent) so the acrylic frosted-glass shows through.

### Decisions
- DEC-027: APPROVED + SHIPPED (Option A — full DComp rewrite, both bars).
- DEC-022 / DEC-023: now addressed by DEC-027. Real acrylic via DWMSBT_TRANSIENTWINDOW; entrance animation via SetWindowPos + DComp (no per-frame re-render).

---

## [0.6.0] — 2026-04-18
> Phase 6 partial — Polish (incremental additions only; blur + slide-up + window-animator deferred)

### Added
- `src/system/CompositionHelper.h` — shared header-only helper. Currently exports `Composition::GetPrimaryMonitorSize` (DEC-025) via `GetMonitorInfo(MonitorFromPoint({0,0}, MONITOR_DEFAULTTOPRIMARY))`. Also holds stubs for the two blur APIs that proved non-functional on Win11 (`ApplyBlurBehind`, `EnableDwmBlur`) — kept for reference but no callers remain.
- `MenuBarWindow` — DEC-024 inline Now-Playing text rendered between active-app name and the right widget cluster. Mutex-protected `m_nowPlayingText` populated by the existing SMTC MTA worker via `TryGetMediaPropertiesAsync().get()`; drawn with `StringTrimmingEllipsisCharacter` at 220 logical px, dimmed foreground colour (`kTrackFg`).
- `AppBarManager` — work-area reservation routed through `Composition::GetPrimaryMonitorSize` in both `Apply()` and `WM_DISPLAYCHANGE` (DEC-025).

### Deferred
- **DEC-022 (real blur)** — investigated both `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` and `DwmEnableBlurBehindWindow` in combination with `WS_EX_LAYERED` + `UpdateLayeredWindow`. Microsoft has neutered both APIs on Windows 11: the backdrop is reserved but no actual blur is applied, leaving a solid tint (white or dark depending on API) behind the window. Real acrylic requires a DirectComposition rewrite (abandon `WS_EX_LAYERED`) — scoped for a future session.
- **DEC-023 (dock slide-up entrance)** — tried three approaches: (a) `WM_TIMER` 16ms with `InvalidateRect`, (b) `WM_TIMER` with `RedrawWindow(RDW_UPDATENOW)`, (c) synchronous `Sleep`-loop pumping messages with `SetWindowPos` on the cached layered bitmap. None produced visible motion on Bala's Win11 build — dock either appeared at target instantly or caused the whole overlay to stay invisible. Requires deeper investigation, likely coupled with the DirectComposition rewrite.
- **DEC-026 (macOS-style open-window animation for foreign apps)** — prototyped a `WindowAnimator` module that hooked `EVENT_OBJECT_SHOW`, added `WS_EX_LAYERED` to new top-level windows, and animated scale+fade. Dropped: didn't trigger reliably, UIPI blocks elevated windows, and Bala decided to stop pursuing it. Files removed.

### Decisions
- DEC-022: PENDING (deferred pending DirectComposition rewrite).
- DEC-023: PENDING (deferred pending DirectComposition rewrite).
- DEC-024: APPROVED + SHIPPED.
- DEC-025: APPROVED + SHIPPED.
- DEC-026: REJECTED (insufficient reliability on Win11; dropped mid-session).

### Changed
- `MenuBarWindow::OnPaint` kept on the BitBlt + `SetLayeredWindowAttributes(LWA_ALPHA, 255)` path from Phase 5. Background gradient restored to opaque dark grey (`kBarTop/Mid/Bot` alpha = 255) since the short-lived per-pixel-alpha + blur experiment reverted.
- `DockWindow::Show` simplified back to `ShowWindow(SW_SHOWNOACTIVATE)` + `UpdateWindow` — no entrance animation.

---

## [0.5.0] — 2026-04-18
> Phase 5 complete — Menu Bar

### Added
- `SystemInfo::GetBattery` — real implementation via `GetSystemPowerStatus`, returns percent + charging state.
- `SystemInfo::GetVolume` — COM-based default-audio-endpoint query via `IMMDeviceEnumerator` + `IAudioEndpointVolume`; returns scalar volume + mute state.
- `SystemInfo::GetWifi` — `WlanOpenHandle` + `WlanEnumInterfaces` + `WlanQueryInterface(wlan_intf_opcode_current_connection)`; returns SSID + signal quality.
- `ActiveAppWatcher` — real implementation (DEC-020). Extracts version-info `FileDescription` (e.g. "Google Chrome" instead of "chrome"), falls back to capitalized exe basename. Exe-path → name cache avoids repeated version lookups. Class filter splits into desktop (Progman/WorkerW/CabinetWClass → reset to "Dock") vs own-overlay (macOSWin_* → preserve); also filters shell exes (explorer, searchhost, startmenuexperiencehost, shellexperiencehost).
- `SystemInfoBar::Fetch` / `Render` — GDI+ vector-rendered Wi-Fi arcs, speaker + sound-wave volume glyph, battery outline with fill + charging bolt, macOS-format clock (`Ddd D Mon  HH:MM`). All anti-aliased and DPI-scaled per-frame via `g_scale = GetDpiForSystem() / 96`.
- `MenuBarWindow` — double-buffered GDI+ with bold Segoe UI app name. Media-control cluster (Prev / Play-Pause / Next) using Segoe Fluent Icons glyphs + `SendInput(VK_MEDIA_*)`. Real play/pause state driven by SMTC (`GlobalSystemMediaTransportControlsSessionManager` via C++/WinRT, polled on a dedicated MTA background thread and surfaced through `std::atomic<bool> m_isPlaying`). DPI-aware drawing via `m_dpiScale`. Seelen-style two-stop gradient with top highlight + bottom shadow. 2s refresh timer drives clock + widgets.
- `DockWindow` — switched from chroma-key (`LWA_COLORKEY` magenta) to per-pixel alpha (`UpdateLayeredWindow(AC_SRC_ALPHA)` with 32bpp top-down DIB), unlocking real translucency. Rounded pill panel behind icons at ~60% alpha. Running indicator dots switched from GDI `Ellipse` to `Gdiplus::FillEllipse` (GDI writes alpha=0 under AC_SRC_ALPHA). Full DPI scaling via `m_dpiScale` + `S()` helper. `.lnk` shortcuts now resolve to target exe basename (cached per pin) so running dots light up for Start-menu-pinned apps.
- Process-level DPI awareness — `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` set before any window creation; AppBar work-area reservation scaled via `MulDiv`.
- `SystemInfoData::wifiQuality` — threads Wi-Fi signal strength from `WlanQueryInterface` through to the arc renderer.
- `version.lib` linked (for `GetFileVersionInfoW`/`VerQueryValueW`).
- `windowsapp.lib` linked (C++/WinRT for SMTC).

### Decisions
- DEC-017: clock format `Ddd D Mon  HH:MM`, refreshed every 2s (same tick as other widgets).
- DEC-018: battery + volume + Wi-Fi + clock all in v1.
- DEC-019: widgets static (no click behavior) in v1.
- DEC-020: `SetWinEventHook` + FileDescription active app name source.
- DEC-021: media-control cluster in v1 (superseded static Apple glyph per Bala's in-session direction).

---

## [0.4.0] — 2026-04-17
> Phase 4 complete — Animation

### Added
- Fish-eye hover magnification: cosine falloff, 1.8× max at cursor, 120px radius (DEC-016).
- 16ms `WM_TIMER` animation loop drives per-frame GDI+ bicubic scaling (DEC-014, DEC-015).
- `DOCK_WINDOW_HEIGHT=72` — dock window taller than visible zone; extra transparent headroom lets magnified icons grow upward without clipping.
- Icons bottom-anchored (`ICON_BOTTOM_Y`) so magnification grows upward, matching macOS.
- Z-sorted draw order: smallest-scale icons drawn first so the most-magnified icon renders on top.
- `WM_ERASEBKGND` suppressed — `OnPaint` owns the entire background.
- Double-buffered `OnPaint` — all drawing to off-screen DC, committed with a single `BitBlt` to eliminate frame flicker.
- `TIMER_ANIMATE` handler polls `GetCursorPos` every 16ms as the authoritative cursor-outside check, handling spurious `WM_MOUSELEAVE` from `LWA_COLORKEY` transparent pixels.
- Immediate `InvalidateRect` on `WM_MOUSEMOVE` for snappy response.

## [Previous releases below]

### Planned (Phase 1 — Skeleton)
- Two borderless always-on-top windows (Dock + Menu Bar)
- Taskbar auto-hide on launch, restore on exit
- System tray icon with Quit option

### Planned (Phase 2 — Dock Core)
- Config loading from `pinned_apps.json`
- Static app icons rendered in Dock
- Click icon to launch app
- Auto-focus if app is already running

### Planned (Phase 3 — Interaction)
- Drag `.exe` / `.lnk` onto Dock to pin
- Auto-extract icon from dragged file
- Running app indicator dots (via EnumWindows polling)
- Config auto-saved on every change

### Planned (Phase 4 — Animation)
- Dock hover magnification (fish-eye, ~1.8x scale at cursor, 120px radius)
- Smooth 60fps animation via Direct2D

### Planned (Phase 5 — Menu Bar)
- Live clock (updates every second)
- Battery level + charging indicator
- Volume level indicator
- Wi-Fi connection status
- Active foreground app name (via SetWinEventHook)

### Planned (Phase 6 — Polish)
- Acrylic/blur background on Dock and Menu Bar (DirectComposition)
- Dock entrance animation on launch
- Edge case handling (multi-monitor, display resolution changes)
- Crash recovery (taskbar always restored)

---

## [0.1.0] — 2026-04-14
> Phase 1 complete — Skeleton

### Added
- `DockWindow` — borderless, always-on-top, layered, centered at screen bottom. Placeholder black fill. Accepts dropped files (Phase 3 hook).
- `MenuBarWindow` — full-width, always-on-top, layered, pinned to top. Placeholder clock (updates every second), active app name, Apple glyph placeholder.
- `TaskbarManager` — hides `Shell_TrayWnd` + `Shell_SecondaryTrayWnd` on launch; restores both on exit.
- `main.cpp` — RAII guards (`TaskbarHideGuard`, `ComApartment`) ensure taskbar is always restored on any exit path. Propagates non-zero exit code on window creation failure.
- Full `CMakeLists.txt` — FetchContent for nlohmann/json, links all required Windows libraries, static CRT, `/W4` warnings.

---

## [0.2.0] — 2026-04-15
> Phase 2 complete — Dock Core

### Added
- `ConfigManager::Load()` wired into `DockWindow::Create()` — pinned apps populate the dock on startup.
- `DockWindow::AddIcon` — `SHGetFileInfoW`-based icon extraction (handles `.exe` and `.lnk` correctly — returns the target app's icon, not the shortcut's). `IDI_APPLICATION` fallback if extraction fails.
- `DockWindow::Reposition` — dynamic dock width = `(N+1)·ICON_PADDING + N·ICON_SIZE`, recenters at screen bottom, assigns per-icon bounds. Empty list falls back to a 120px placeholder bar.
- `DockWindow::OnPaint` — renders each pinned icon via `DrawIconEx` scaled to its bounds rect.
- `DockWindow::OnLButtonUp` — click hit-test → `AppLauncher::LaunchOrFocus`.
- `DockWindow::RemoveIcon` — index-bounded erase + reposition (trigger wiring lands in Phase 3).
- `AppLauncher::IsUserAppWindow` — filter that excludes shell-background windows (`Progman`, `WorkerW`, `Shell_TrayWnd`, etc.) from the "is this app running?" enumeration. Without it, clicking the `explorer.exe` tile would focus the desktop instead of opening a File Explorer window.
- `ConfigManager` gtest suite — 7 tests (missing-file, auto-create-on-missing, round-trip, malformed JSON, empty-path skipped, Unicode round-trip, empty-list save).

### Fixed
- `DockWindow::AddIcon` fallback path: `LoadIconW(nullptr, IDI_APPLICATION)` compile error — `IDI_APPLICATION` is `LPSTR`, `LoadIconW` needs `LPCWSTR`. Cast via `reinterpret_cast<LPCWSTR>`.

---

## [0.3.0] — 2026-04-17
> Phase 3 complete — Interaction

### Added
- `DockDropTarget` (new) — OLE `IDropTarget` replacing `WM_DROPFILES`. Handles files from Explorer (CF_HDROP), apps from Start Menu (Shell IDList Array + app ID resolution), and UWP/Store apps (`shell:AppsFolder\<appId>`).
- `DockDropValidator` (new) — pure-logic file-type validation and name extraction, unit-testable without Win32 UI dependencies. Accepts `.exe`, `.lnk`, and `shell:AppsFolder\` paths.
- `DockWindow::HasIcon` — case-insensitive duplicate detection.
- `DockWindow::OnDropComplete` — saves config on successful pin, flashes red if all dropped files rejected.
- `DockWindow::FlashReject` — 200ms red background flash for rejected drops.
- `DockWindow::SaveConfig` — persists icon list to `pinned_apps.json` on every pin/unpin.
- Drag-off-dock unpin (DEC-012) — click+drag icon off dock, release outside bounds removes it and saves config.
- Process monitor polling — 1500ms `WM_TIMER` polls `ProcessMonitor::GetRunningAppNames`, draws white indicator dot below running app icons.
- 17 gtest cases for `DockDropValidator` (file-type validation + name extraction).

### Fixed
- `OleInitialize` replaces `CoInitializeEx` — required for `RegisterDragDrop` to work (plain COM init is insufficient for OLE drag-drop).
- `.lnk` icons no longer show the shortcut arrow overlay — `AddIcon` resolves `.lnk` target exe via `IShellLink` and extracts the icon from the target instead.
- UWP/Store apps (Settings, Microsoft Store, etc.) pin correctly via `shell:AppsFolder\<appId>` with icons extracted via `IShellItemImageFactory`.
- Start Menu app ID matching — stricter token matching with generic-word blacklist prevents false matches (e.g. Settings no longer maps to Administrative Tools).

## [0.3.1] — 2026-04-17
> Post-Phase-3 bug fixes (Session 007)

### Fixed
- **Work area not applied to already-maximized windows on launch** — `AppBarManager::Apply()` now enumerates existing maximized windows and posts them `WM_SETTINGCHANGE(SPI_SETWORKAREA)` directly (skipping Explorer shell windows) so they snap to the reserved bars immediately, without the minimize-restore cycle previously required.
- **Settings (UWP) icon looks wrong** — `ExtractUwpIcon` now requests 256×256 from `IShellItemImageFactory::GetImage` (was 48×48) with `SIIGBF_RESIZETOFIT` instead of `SIIGBF_ICONONLY`. The full tile artwork (coloured background + icon) now renders correctly.
- **Dock icons pixelated** — `AddIcon` now uses `IShellItemImageFactory::GetImage` at exactly `ICON_SIZE×ICON_SIZE` for regular exe/lnk icons, eliminating the blurry upscaling from the 32×32 `SHGFI_LARGEICON` path. `SHGFI_LARGEICON` kept as fallback.
- **Dock icons too large** — `ICON_SIZE` reduced 56→48 px (standard macOS default-dock icon size, matches natural Win32 icon resolution tier).

---

## [0.3.2] — 2026-04-17
> Session 008 — Dock visual polish (no new features; Phase 3 scope)

### Added
- GDI+ rendering apartment (`GdiplusStartup`/`GdiplusShutdown`) in `main.cpp` for bicubic icon scaling.
- `FetchJumboIcon` helper — pulls 256×256 HICON from system `SHIL_JUMBO` imagelist.
- `FetchShellBitmap` helper — `IShellItemImageFactory::GetImage` with `SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK`, falls back to tile-logo flag on failure.
- `BitmapToIcon` helper — wraps a 32bpp HBITMAP into an HICON so `Gdiplus::Bitmap::FromHICON` renders it with clean alpha.
- CMake: `msimg32`, `gdiplus` linked.

### Changed
- `DockWindow::OnPaint` now renders icons via `Gdiplus::Graphics::DrawImage` with `InterpolationModeHighQualityBicubic`. `DrawIconEx`/`AlphaBlend` paths removed — the former ignores stretch mode, the latter is nearest-neighbour.
- UWP icon path prefers factory tile → falls back to JUMBO (previously reversed). Settings now shows its real package tile logo.
- Regular (.exe/.lnk) icon path prefers JUMBO → factory → `SHGetFileInfoW(SHGFI_LARGEICON)`.
- Dock transparency: `LWA_COLORKEY(RGB 255,0,255)` replaces `LWA_ALPHA(240)`. Dock background filled with magenta = fully transparent; only icons render.
- Dock geometry: `ICON_SIZE` 48→28, `DOCK_HEIGHT` 76→40, `ICON_PADDING` 10→20, `BOTTOM_GAP` 8→2. `main.cpp` `DOCK_TOTAL_HEIGHT` reserve 84→42.

### Fixed
- Icon scaling quality: bicubic interpolation eliminates the blocky/pixelated rendering present in 0.3.1.
- Windows 11 Settings icon: displays the real colored tile logo instead of a blank/white square.
- Visible black bar behind the dock: eliminated via color-key transparency.

---

## [1.0.0] — TBD
> Phase 6 complete — Full v1 release

---

*Last updated: 2026-04-18 | Session 012 — v0.6.0 shipped (Now-Playing + multi-monitor only). Blur, slide-up, and window-animator deferred/dropped.*
