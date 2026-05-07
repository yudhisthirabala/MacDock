# Decision Log — macOS Windows Overlay

> All architectural and product decisions must be logged here before being implemented.
> Claude Code must not implement any decision with status PENDING.

---

## Decision Template

```
### DEC-XXX — [Short Title]
- **Date:** YYYY-MM-DD
- **Session:** XXX
- **Raised by:** Claude Code / Bala
- **Status:** PENDING | APPROVED | REJECTED | SUPERSEDED
- **Context:** Why this decision is needed
- **Options considered:**
  - Option A: ...
  - Option B: ...
- **Decision:** Which option was chosen
- **Approved by:** Bala (via chat on YYYY-MM-DD)
- **Notes:** Any caveats or follow-up actions
```

---

## Decisions

### DEC-001 — Target Platform
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Bala
- **Status:** APPROVED
- **Context:** Needed to determine which OS(es) the app targets.
- **Options considered:**
  - Windows only
  - Windows + Linux
  - Cross-platform (all)
- **Decision:** Windows only
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Enables use of Win32-specific APIs throughout.

---

### DEC-002 — Integration Depth
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** How deeply should the app modify the Windows shell?
- **Options considered:**
  - Visual overlay only (non-invasive)
  - Full shell replacement
  - Middle ground (hide taskbar, add Dock + menu bar, apply theming)
- **Decision:** Visual overlay only — add Dock and menu bar as floating windows, hide Windows taskbar on launch and restore on exit.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Safest approach. No risk of breaking Windows shell.

---

### DEC-003 — Tech Stack
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** What technology to use to build the app.
- **Options considered:**
  - Electron (JS/TS) — cross-platform, web-based UI, heavy
  - C# / .NET / WPF — native Windows, moderate
  - Python + Qt — quick prototype, less native
  - Raw C++ with Win32 + Direct2D — native, zero dependencies
  - Qt6 with C++ — native C++, good abstractions, requires Qt runtime
- **Decision:** Raw C++ with Win32 + Direct2D + DirectComposition. No Qt. Self-contained `.exe`.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** User confirmed `.exe` is a hard requirement. Claude Code handles the verbosity of Win32 boilerplate.

---

### DEC-004 — Dock Position
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Where should the Dock be positioned horizontally?
- **Options considered:**
  - Centered (macOS default)
  - Full-width (Windows taskbar style)
- **Decision:** Centered, floating slightly above the bottom edge.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Matches authentic macOS appearance.

---

### DEC-005 — Dock Default State
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Should the Dock be pre-loaded with common apps or start empty?
- **Options considered:**
  - Pre-loaded with common Windows apps (Chrome, Explorer, etc.)
  - Starts empty, user drags apps in to pin
- **Decision:** Starts empty. User drags `.exe` or `.lnk` files onto the Dock to pin.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Gives user full control. No assumptions about what apps are installed.

---

### DEC-006 — Config Persistence Format
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** How to persist pinned app layout between sessions.
- **Options considered:**
  - Windows Registry
  - INI file
  - JSON file (via nlohmann/json)
- **Decision:** JSON file (`src/config/pinned_apps.json`) using nlohmann/json (header-only).
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Human-readable, easy to inspect/edit manually. nlohmann/json is header-only so no extra build steps.

---

### DEC-007 — Dock Magnification in v1
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Whether to include hover magnification animation in the first version.
- **Options considered:**
  - Include in v1
  - Defer to later phase
- **Decision:** Include in v1 (Phase 4 of the build plan).
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Icons within ~120px of cursor scale up to ~1.8x. Adjacent icons scale proportionally (fish-eye lens effect).

---

### DEC-008 — Global App Menu Bar
- **Date:** 2026-04-14
- **Session:** 001 (Planning)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Should the menu bar show the active app's menus (File, Edit, View...) in v1?
- **Options considered:**
  - Full global menu (intercept active app's Win32 menus) — very complex
  - Simple version: show active app name only, no menus
- **Decision:** Deferred to v2. v1 shows active app name in center of menu bar only.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Full global menu requires MSAA/UI Automation to read app menus. Scoped out of v1.

---

### DEC-009 — Quit Mechanism
- **Date:** 2026-04-14
- **Session:** 002 (original), revised Session 003
- **Raised by:** Claude Code
- **Status:** APPROVED (revised)
- **Context:** Phase 1 has no quit UI — the app can only be killed via Task Manager. A quit path is required before Phase 2 so testers can safely stop the app without killing their shell.
- **Options considered:**
  - Option A: System tray icon with a right-click "Quit" menu
  - Option B: Global hotkey (e.g. Ctrl+Alt+Q) with no visible quit UI
  - Option C: Both — tray icon primary, hotkey as backup
- **Original Decision (Session 002):** Option A — tray icon only.
- **Revised Decision (Session 003):** **Option C** — global hotkey **Ctrl+Alt+Q** is the primary quit path, tray icon remains registered as a secondary fallback.
- **Reason for revision:** During Session 003 smoke test the tray icon never appeared. `TaskbarManager::Hide()` hides `Shell_TrayWnd`, which *contains* the notification area — so with the taskbar hidden the tray has nowhere to render. The "^" overflow chevron is also gone. Option A was silently broken under Phase 1's taskbar-hide behavior. The hotkey bypasses the tray entirely and works regardless of taskbar visibility.
- **Approved by:** Bala (via chat on 2026-04-14, original); Bala (via chat on 2026-04-14, revision)
- **Notes:** `RegisterHotKey(MOD_CONTROL | MOD_ALT, 'Q')` + `WM_HOTKEY` handler in `TrayIcon`'s hidden receiver window. Tray icon code stays as-is — it becomes visible automatically once the taskbar-hide behavior is moved/refined in Phase 6. Unblocks T1-005, T1-006, T1-009, T1-010.

---

### DEC-010 — Automated Unit Test Framework
- **Date:** 2026-04-14
- **Session:** 002
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Manual testing covers Win32 UI behavior but cannot automatically verify pure logic (config parsing, path validation, data calculations). An automated unit test layer catches regressions early and gates each phase before it is marked complete.
- **Options considered:**
  - Option A: Google Test (gtest) via FetchContent — industry standard, CMake-native, verbose output
  - Option B: Catch2 — header-only, simpler syntax, smaller ecosystem
  - Option C: No automated tests — manual only
- **Decision:** Google Test via FetchContent. Separate `tests/` CMake target. Run with `ctest` as part of each phase verification gate.
- **Approved by:** Bala (via chat on 2026-04-14)
- **Notes:** Testable components in v1: `ConfigManager` (Phase 2), `AppLauncher` path validation (Phase 2), `SystemInfo` data parsing (Phase 5). Win32 UI code (windows, taskbar, rendering) stays as manual tests in `TESTING.md`. Test scaffolding added at start of Phase 2, not before.

---

### DEC-011 — Drag-to-Pin Semantics
- **Date:** 2026-04-15
- **Session:** 005
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Phase 3 begins with `DockDropHandler` (`WM_DROPFILES` → `AddIcon` + `ConfigManager::Save`). Before writing the handler we need to pin down four sub-questions so the behavior is consistent and testable.
- **Sub-questions + options:**
  1. **Accepted file types**
     - Option A: `.exe` and `.lnk` only (strict, matches Session 001 plan)
     - Option B: A + `.bat` / `.cmd` (scripts — `ShellExecute` handles them; icons are generic)
     - Option C: A + `.url` (internet shortcuts — useful for "pin a website")
     - Option D: Accept anything `ShellExecute` can open (most permissive — includes folders, docs)
  2. **Rejection feedback**
     - Option A: Silent reject (drop does nothing, cursor returns)
     - Option B: Brief visual cue — tile flashes red for ~200ms, no dialog
     - Option C: `MessageBox` with "File type not supported"
  3. **Duplicate-path handling** (user drops an already-pinned app)
     - Option A: Silently ignore the duplicate
     - Option B: Move the existing tile to the drop position (reorder)
     - Option C: Allow the duplicate (two tiles for the same app)
  4. **Multi-drop** (user drops 5 files at once)
     - Option A: Add all valid ones in the order they were dropped, skip invalid silently
     - Option B: Add all valid, show one summary message for skipped
- **Claude's recommendation:** 1A (strict `.exe`+`.lnk`), 2B (flash red — macOS-authentic, no dialog interrupt), 3A (silent ignore — matches macOS Dock), 4A (silent skip). Rationale: matches the macOS Dock's real behavior and keeps the Phase 3 scope tight; `.url`/`.bat` can be added later without breaking anything.
- **Decision:** 1A + 2B + 3A + 4A (Claude's recommendation adopted in full).
- **Approved by:** Bala (via chat on 2026-04-15)
- **Notes:** Affects `DockDropHandler.cpp` and test cases T3-001..T3-004.

---

### DEC-012 — Unpin Gesture
- **Date:** 2026-04-15
- **Session:** 005 (deferred since Session 001)
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Users must be able to remove a pinned app. Needed by Phase 3 (T3 test row implicitly assumes this exists before Phase 3 can be declared done). `DockWindow::RemoveIcon` already exists from Session 004; it just has no trigger.
- **Options considered:**
  - Option A: Right-click a tile → context menu with "Remove from Dock"
  - Option B: Drag a tile off the Dock — tile follows cursor, releasing outside the Dock bounds deletes it (macOS behavior, with a "poof" animation in real macOS)
  - Option C: Both — right-click as the primary, drag-off as a convenience
- **Claude's recommendation:** **Option B** for authentic macOS feel in v1. Option C is tempting but doubles the Phase 3 scope (context menu rendering + hit-test + command routing) for marginal UX gain. A right-click menu can be added in Phase 6 polish if needed.
- **Decision:** Option B — drag-off-dock removes the tile.
- **Approved by:** Bala (via chat on 2026-04-15)
- **Notes:** Affects `DockWindow::WndProc` (`WM_LBUTTONDOWN` → capture + track; `WM_MOUSEMOVE` → move tile; `WM_LBUTTONUP` → hit-test bounds, remove if outside). No new file needed — all within `DockWindow`. Phase 4 magnification (DEC-007) will share the same mouse-tracking plumbing, so building it with drag-off in mind keeps the code coherent. Drag-off interacts with Phase 4 animation — if a drag is in progress we should suppress magnification.

---

### DEC-013 — Reserve Screen Work Area for Dock + Menu Bar
- **Date:** 2026-04-15
- **Session:** 005
- **Raised by:** Claude Code (triggered by Bala-reported bug: maximized windows cover the top menu bar; user can't reach the title bar's close button)
- **Status:** APPROVED
- **Context:** Our Dock and MenuBar are `WS_POPUP` + `HWND_TOPMOST` — they paint on top of everything, but they do **not** tell Windows to shrink the usable work area. So when a user maximizes an app (Notepad, Chrome, etc.), it fills the full monitor including the strip under our MenuBar and the strip under our Dock, and its title-bar close/minimize buttons end up hidden behind our MenuBar. On macOS, the menu bar and Dock are screen-reserved regions that maximized windows respect automatically. We need the Windows equivalent.
- **Options considered:**
  - **Option A — Register both windows as AppBars via `SHAppBarMessage`.** Native Windows mechanism (`ABM_NEW` → `ABM_SETPOS`), maintained since Win95 precisely for this use case. Windows itself subtracts the AppBar rect from `SPI_GETWORKAREA` for all apps — maximize, Win+Up snap, and window arrangement APIs all honor it. Clean-up on exit via `ABM_REMOVE`; the registration is process-lifetime-scoped so crashes don't leave a reserved region stranded (Explorer cleans up dead AppBars). **Downside:** AppBar messages are mildly chatty — we own `ABN_POSCHANGED`/`ABN_FULLSCREENAPP` callbacks; need per-monitor recomputation on display changes.
  - **Option B — Manually adjust `SPI_SETWORKAREA` via `SystemParametersInfo`.** One-line call, reserve a rect. **Downsides:** (1) change is *global* and persists across processes, (2) if we crash before restoring, the user's work area stays shrunk until reboot, (3) doesn't handle multi-monitor cleanly, (4) conflicts with the real taskbar if we ever stop hiding it.
  - **Option C — Hook `WM_GETMINMAXINFO` in other apps.** Requires DLL injection into every process. Off the table for a non-invasive overlay (violates DEC-002).
  - **Option D — Do nothing; accept the occlusion.** User-visible bug; rejected.
- **Sub-question — Dock reservation shape:**
  - A1: Reserve a full-width bottom strip (macOS-authentic, simple)
  - A2: Reserve only the centered Dock-width region (complex fake-AppBar trick)
- **Claude's recommendation:** **Option A** — register Dock and MenuBar as AppBars. It's the sanctioned Windows API for exactly this purpose, survives crashes cleanly (no global-state pollution), handles multi-monitor via the `uEdge` + monitor-rect protocol, and integrates with snap/maximize for free. The extra message-handling code (`ABN_POSCHANGED` for display/monitor changes, `ABN_FULLSCREENAPP` so our bars yield to true fullscreen apps like games/video) is ~50 lines per window and belongs in a dedicated `AppBarManager` helper to keep it out of the window classes.
- **Decision:** **Option A + sub-choice A1** — register both windows as AppBars; Dock reserves a full-width bottom strip.
- **Approved by:** Bala (via chat on 2026-04-15)
- **Notes:** Implementation plan:
  - New file pair `src/system/AppBarManager.{h,cpp}` — RAII wrapper: constructor does `ABM_NEW` + initial `ABM_QUERYPOS`/`ABM_SETPOS`; destructor does `ABM_REMOVE`. Exposes a callback for `ABN_POSCHANGED` so Dock/MenuBar can resize themselves if Windows asks (e.g. user plugs in a different-resolution monitor).
  - `DockWindow` and `MenuBarWindow` each own an `AppBarManager` member instantiated after their HWND exists.
  - Edge choices: MenuBar = `ABE_TOP`, Dock = `ABE_BOTTOM`. Dock-width is dynamic, but AppBars reserve a full-width strip regardless of the window's visible width (this matches macOS: the Dock's full-height reserved strip runs screen edge-to-edge even though only the centered pill is visible). If you want the reservation to track the visible Dock width only, we'd need a different approach (fake full-width transparent AppBar + visible child) — flag and discuss.
  - Bonus: fixes the currently-untestable T1-005/T1-006 (always-on-top vs maximized window) because maximized windows will now stop at our bars, which is the actual desired behavior.

---

## Pending Decisions

*No pending decisions at this time.*

<!-- DEC-017..DEC-021 approved below (Session 010) -->


---

### DEC-014 — Phase 4 Frame Rate Mechanism
- **Date:** 2026-04-17
- **Session:** 009
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** The fish-eye magnification animation needs to drive redraws at ~60fps while the cursor is inside the dock. We need a timer/loop mechanism that fires at 16ms intervals without blocking the message pump.
- **Options considered:**
  - **Option A: `WM_TIMER` at 16ms** — simple, already used for the process-monitor poll. `SetTimer(hwnd, TIMER_ANIMATE, 16, NULL)`; each `WM_TIMER` recomputes the magnification scale and calls `InvalidateRect`. Accurate to ~15ms on most systems (Windows timer resolution). No extra thread. Con: WM_TIMER is a low-priority message — under heavy load a frame may be skipped, but at 16ms the gap is invisible to users.
  - **Option B: Direct2D render loop on a dedicated thread** — a background thread calls `ID2D1HwndRenderTarget::BeginDraw`/`EndDraw` in a tight loop, sleeping via `QueryPerformanceCounter`. Theoretically more accurate, GPU-composited. Con: requires moving all rendering to Direct2D (big Phase 4 scope increase), thread synchronization for the icon list, and is complete overkill for a 60fps icon-scale effect on a floating pill window.
- **Claude's recommendation:** **Option A** — `WM_TIMER` at 16ms. Matches how real macOS dock animations work (display-sync is unimportant here), keeps the rendering single-threaded, and is consistent with the existing `TIMER_PROCESS_MONITOR` pattern. If we ever need GPU compositing we can revisit in Phase 6 when DirectComposition lands.
- **Decision:** Option A — `WM_TIMER` at 16ms.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** Timer only runs while cursor is inside the dock (`WM_MOUSEMOVE` starts it, `WM_MOUSELEAVE` stops it + triggers a final "restore to normal" frame). Avoids continuous 60fps redraws when the dock is idle.

---

### DEC-015 — Rendering Backend for Animated Icon Scaling
- **Date:** 2026-04-17
- **Session:** 009
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Session 008 introduced GDI+ (`Gdiplus::Graphics::DrawImage` with `InterpolationModeHighQualityBicubic`) for static icon rendering. For Phase 4 each frame must redraw icons at dynamically computed sizes (up to 1.8× the base 28px). We need to decide whether to keep GDI+ or switch the animation path to Direct2D.
- **Options considered:**
  - **Option A: Keep GDI+ for all rendering (static + animated)** — reuse the existing `OnPaint` pipeline. Compute each icon's scaled rect per frame, call `DrawImage` at that size. GDI+ bicubic handles arbitrary destination rects cleanly. Con: GDI+ is a CPU rasterizer; at 60fps with ~8 icons each redrawn at a different size, CPU cost is measurable but not significant on modern hardware for a 200px-wide window.
  - **Option B: Switch animated path to Direct2D `ID2D1Bitmap::DrawBitmap`** — GPU-accelerated scaling, true `ID2D1HwndRenderTarget`. Con: requires a full second rendering path alongside GDI+, COM lifecycle management for bitmaps, and is significant scope for a window that is at most ~300px wide. DEC-003 deferred Direct2D to Phase 6 for compositing; pulling it in now only for scaling is premature.
- **Claude's recommendation:** **Option A** — extend GDI+ for animated frames. The dock window is tiny; GDI+ bicubic at 60fps for ≤10 icons in a ~300px window won't produce a measurable CPU spike. Direct2D arrives in Phase 6 anyway; keeping Phase 4 in GDI+ avoids a parallel rendering path and deferred refactor debt.
- **Decision:** Option A — keep GDI+ for all rendering.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** `OnPaint` already receives a `PAINTSTRUCT` HDC; wrapping it in `Gdiplus::Graphics` per frame is a one-liner. Each icon's destination rect will be computed from its center + scaled half-size, keeping adjacent icons centered on their slot centers (macOS fish-eye behavior).

---

### DEC-016 — Magnification Curve Shape
- **Date:** 2026-04-17
- **Session:** 009
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** As the cursor moves across the dock, each icon should scale based on its distance from the cursor. The curve shape determines how "soft" vs "sharp" the magnification falloff feels. Already established in DEC-007: max scale 1.8×, radius 120px. This decision pins the math.
- **Options considered:**
  - **Option A: Linear falloff** — `scale = 1.0 + 0.8 * max(0, 1 - dist/120)`. Simple; produces a visible "cone" shape that feels mechanical — scale drops in a straight line from center to edge. The macOS dock doesn't use this.
  - **Option B: Cosine falloff** — `scale = 1.0 + 0.8 * 0.5 * (1 + cos(π * dist/120))` for `dist ≤ 120`, else 1.0. Smooth ease-in/ease-out: large at center, tapers gently toward the radius boundary, zero derivative at both ends. This is the closest match to the real macOS dock behavior and is what Apple uses internally.
  - **Option C: Gaussian falloff** — `scale = 1.0 + 0.8 * exp(-(dist²)/(2·40²))`. Very soft peak, infinite tail (clamp at 120px). Slightly more "blob-like" than cosine; slightly more expensive (exp vs cos).
- **Claude's recommendation:** **Option B — cosine falloff**. It matches the authentic macOS feel, has zero derivative at the edges (no visual "pop" when an icon enters/exits the influence radius), and is a single `cos()` call per icon per frame. Formula is already in scope as the `MAGNIFY_RADIUS` / `MAGNIFY_MAX` constants confirm this intent.
- **Decision:** Option B — cosine falloff.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** Formula per icon: `float t = dist / MAGNIFY_RADIUS; scale = (t >= 1.0f) ? 1.0f : 1.0f + (MAGNIFY_MAX - 1.0f) * 0.5f * (1.0f + cosf(M_PI_F * t))`. Cursor position tracked in client coords via `WM_MOUSEMOVE`; icon center derived from `Reposition()`-assigned bounds. No change to `MAGNIFY_RADIUS` or `MAGNIFY_MAX` constants.

---

### DEC-017 — Clock Format & Update Cadence
- **Date:** 2026-04-17
- **Session:** 010
- **Status:** APPROVED
- **Context:** Phase 5 menu bar needs a clock widget. Format + refresh rate must be pinned before implementation.
- **Options considered:**
  - A: 24-hour `HH:MM`, minute-boundary tick.
  - B: 12-hour `h:mm AM/PM`.
  - C: macOS default: `Thu 17 Apr  14:32` (weekday + date + time).
- **Decision:** C — macOS-authentic `Ddd DD Mon  HH:MM` format. Refresh every 30s via `WM_TIMER`.
- **Approved by:** Bala (via chat on 2026-04-17 — "choose the best thing... look like mac os")
- **Notes:** Rendered right-aligned in menu bar, after all system-info widgets. `GetLocalTime` + `GetDateFormatW(LOCALE_USER_DEFAULT, 0, ..., L"ddd dd MMM")` + `GetTimeFormatW(..., L"HH:mm")`.

---

### DEC-018 — System Info Widget Scope for v1
- **Date:** 2026-04-17
- **Session:** 010
- **Status:** APPROVED
- **Context:** Right side of menu bar hosts system status widgets. Need to confirm v1 includes all four vs a subset.
- **Options considered:**
  - A: Battery + Volume + Wi-Fi + Clock (all).
  - B: Battery + Clock only.
  - C: Clock only.
- **Decision:** A — all four widgets.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** Order right-to-left: Clock | Battery | Volume | Wi-Fi. `SystemInfoBar::Fetch` fills all fields; `Render` draws each as icon glyph + small text. Update cadence: 2s timer for battery/volume/Wi-Fi; 30s for clock (via separate timer).

---

### DEC-019 — Widget Interactivity
- **Date:** 2026-04-17
- **Session:** 010
- **Status:** APPROVED
- **Context:** Should menu-bar widgets respond to clicks (volume slider, Wi-Fi picker, etc.) in v1?
- **Options considered:**
  - A: Static display only.
  - B: Click opens the relevant Windows Settings page via `ShellExecute` (e.g. `ms-settings:network`).
  - C: Custom flyout popups (macOS-authentic but large scope).
- **Decision:** A — static display in v1. Clicks do nothing.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** macOS-authentic flyouts deferred to Phase 6 polish. Avoids Phase 5 scope explosion; keeps focus on visual fidelity.

---

### DEC-020 — Active App Name Source
- **Date:** 2026-04-17
- **Session:** 010
- **Status:** APPROVED
- **Context:** macOS menu bar displays the foreground app's *name* (not window title). We need to hook foreground changes and extract a clean name.
- **Options considered:**
  - A: `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` → exe path → version-info `FileDescription` → fallback to exe basename.
  - B: `GetWindowTextW` on foreground HWND.
- **Decision:** A — exe + version-info FileDescription.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** `ActiveAppWatcher` owns the hook. Cache `HWND → name` to avoid re-querying file version on every focus flip. Skip shell windows (Progman, WorkerW, Shell_TrayWnd, our own Dock/MenuBar HWNDs) — in that case keep last-shown name or show "Finder" (macOS-authentic fallback when desktop has focus).

---

### DEC-021 — Apple Logo (Left of Menu Bar)
- **Date:** 2026-04-17
- **Session:** 010
- **Status:** APPROVED
- **Context:** Left end of macOS menu bar is the Apple glyph (clickable → Apple menu). Need an asset and a click behavior.
- **Options considered:**
  - A: Static Apple glyph, no click behavior (placeholder until Phase 6).
  - B: Clickable dropdown (About, Sleep, Restart, Shut Down).
- **Decision:** A — static glyph in v1.
- **Approved by:** Bala (via chat on 2026-04-17)
- **Notes:** Use a PNG at `assets/icons/apple.png` (14px tall, SF-style outline). Rendered via GDI+ DrawImage at left-padding offset. Dropdown deferred to Phase 6.

---

### DEC-022 — Acrylic / Blur Background
- **Date:** 2026-04-18
- **Session:** 012
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Dock and menu bar use solid fills; macOS uses frosted-glass blur-behind. Need a blur mechanism.
- **Options considered:**
  - Option A: `SetWindowCompositionAttribute(WCA_ACCENT_POLICY)` with `ACCENT_ENABLE_BLURBEHIND` — one-call, undocumented but widely used and stable (TranslucentTB, Seelen).
  - Option B: DirectComposition + `IDCompositionBlurEffect` — documented, GPU-composited, ~150–200 lines new COM code per window.
  - Option C: Keep current solid/alpha fill; skip blur.
- **Decision:** Option A — `SetWindowCompositionAttribute` blur-behind with a tint overlay. Most macOS-authentic for minimal implementation cost.
- **Approved by:** Bala (via chat on 2026-04-18 — "chose the option best for me which make it looks like mac")
- **Notes:** Applied to both DockWindow and MenuBarWindow. Dock also keeps the rounded pill paint pass on top for the pill shape. Menu bar keeps the gradient paint on top for the Seelen-style inset-glass look. The `SetWindowCompositionAttribute` HWND attribute must be set after the window is created and visible.

---

### DEC-023 — Dock Entrance Animation
- **Date:** 2026-04-18
- **Session:** 012
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Dock appears instantly on launch; macOS Dock slides up from the bottom edge.
- **Options considered:**
  - Option A: Slide-up on launch — position off-screen, animate upward over ~250ms with ease-out curve via 16ms `WM_TIMER`.
  - Option B: Fade-in over ~200ms. Simpler, less macOS-authentic.
  - Option C: No entrance animation.
- **Decision:** Option A — slide-up ease-out animation matching macOS Dock launch behavior.
- **Approved by:** Bala (via chat on 2026-04-18)
- **Notes:** Reuses `TIMER_ANIMATE` pattern. Ease-out formula: `y = targetY + (startOffsetPx) * (1 - t)^2` where `t` goes 0→1 over 250ms. `startOffsetPx` = dock window height (appears to emerge from below the screen edge). Timer stops when `t >= 1.0`.

---

### DEC-024 — Song Title Display on Media Controls
- **Date:** 2026-04-18
- **Session:** 012
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** Media cluster shows Prev/Play-Pause/Next but no track info. macOS 12+ shows truncated "Song — Artist" inline in the menu bar.
- **Options considered:**
  - Option A: Hover tooltip showing track title + artist via `GetMediaPropertiesAsync`.
  - Option B: Always-visible truncated "Song — Artist" text inline in menu bar between cluster and app name.
  - Option C: No track info display.
- **Decision:** Option B — inline truncated track text, always visible, matching macOS 12+ Now Playing menu bar widget.
- **Approved by:** Bala (via chat on 2026-04-18)
- **Notes:** `GetMediaPropertiesAsync` on the SMTC session already available from the worker thread. Surface title + artist through two `std::wstring` atomics (or a mutex-protected struct) alongside `m_isPlaying`. Menu bar paint truncates to ~180px with an ellipsis. If no media session active, the field is empty (no text shown). SMTC worker polls every 2s alongside play-state — no additional thread needed.

---

### DEC-025 — Multi-Monitor Layout
- **Date:** 2026-04-18
- **Session:** 012
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** All layout math uses `GetSystemMetrics(SM_CXSCREEN/CYSCREEN)` which returns the primary monitor dimensions. On multi-monitor setups the work-area reservation is wrong for secondary monitors and window positioning may be off.
- **Options considered:**
  - Option A: Primary monitor only — no change.
  - Option B: Replace raw `GetSystemMetrics` with `GetMonitorInfo` scoped to the primary monitor HMONITOR. Correct layout without adding multi-bar complexity.
- **Decision:** Option B — use `MonitorFromPoint({0,0}, MONITOR_DEFAULTTOPRIMARY)` + `GetMonitorInfo` for all screen-dimension reads.
- **Approved by:** Bala (via chat on 2026-04-18)
- **Notes:** Affects `DockWindow::Create/Reposition`, `MenuBarWindow::Create`, and `AppBarManager`. One-pass replacement of `GetSystemMetrics` calls. No new behavior — just correct dimensions on primary monitor regardless of display topology.

---

### DEC-027 — DirectComposition Migration for Real Acrylic + Entrance Animation
- **Date:** 2026-04-18
- **Session:** 013
- **Raised by:** Claude Code
- **Status:** PENDING
- **Context:** DEC-022 (blur) and DEC-023 (slide-up entrance) both failed on Windows 11 because `SetWindowCompositionAttribute(ACCENT_ENABLE_BLURBEHIND)` and `DwmEnableBlurBehindWindow` produce only a solid tint (no real blur) when the window uses `WS_EX_LAYERED`. The root constraint: DWM's blur compositor only works on windows that participate in the Desktop Window Manager's redirection surface — layered windows opt out of that surface entirely. The only path to real acrylic on Win11 is to abandon `WS_EX_LAYERED` / `UpdateLayeredWindow` and adopt a DirectComposition visual tree with `WS_EX_NOREDIRECTIONBITMAP`. This is a significant rendering rewrite for both bars.
- **Options considered:**
  - **Option A — Full DirectComposition rewrite (both bars).** Remove `WS_EX_LAYERED` from DockWindow and MenuBarWindow. Set `WS_EX_NOREDIRECTIONBITMAP`. Create a `IDCompositionDevice` → per-window `IDCompositionTarget` → root `IDCompositionVisual`. Render icon/bar content to a `IDCompositionSurface` (or swap chain) each frame. Apply `IDCompositionGaussianBlurEffect` for acrylic. Entrance animation becomes a `IDCompositionAnimation` on the visual's `OffsetY` property — no timer loop needed, DComp drives it on the compositor thread. **Pro:** Real blur, hardware-accelerated animation, clean architecture. **Con:** Significant rewrite (~300–400 lines net new across both windows); DComp requires D3D11 device + DXGI factory initialisation; new COM dependencies (`dcomp.lib`, `d3d11.lib`, `dxgi.lib`). Estimate: 2 focused sessions.
  - **Option B — Defer blur/animation; polish other Phase 6 items first.** Keep the current `WS_EX_LAYERED` + `UpdateLayeredWindow` rendering unchanged (no blur, no entrance animation). Instead spend Phase 6 on lower-risk polish: embed a real multi-resolution `.ico` for the tray icon, improve the Apple/media glyph quality, add display-change (`WM_DISPLAYCHANGE`) robustness, and improve crash recovery. DComp rewrite becomes a "v2.0" scope item. **Pro:** Low risk, ships visible polish quickly. **Con:** No real acrylic; the overlay will continue to look like a flat opaque bar rather than macOS's frosted glass.
  - **Option C — Hybrid: DComp for menu bar only, keep layered dock.** The menu bar is a simpler shape (full-width strip, no per-pixel transparency for dock icons) and acrylic matters more there (macOS menu bar is the most recognisable frosted-glass element). Dock keeps `UpdateLayeredWindow` for the icon per-pixel-alpha path; menu bar gets the DComp rewrite. **Pro:** Smaller scope than A (~1 session); real blur where it's most visible. **Con:** Two different rendering models in the same app; entrance animation still can't use DComp on the dock side.
- **Claude's recommendation:** **Option A** if Bala wants the full macOS look including dock acrylic and smooth entrance animation. **Option B** if the goal is a working v1 shipped sooner. Option C is a reasonable middle ground if the menu bar blur matters most.
- **Decision:** Option A — Full DirectComposition rewrite for both bars.
- **Approved by:** Bala (via chat on 2026-04-18)
- **Notes:** DComp rewrite must land before DEC-022 and DEC-023 are re-attempted. New link dependencies: `dcomp.lib`, `d3d11.lib`, `dxgi.lib`. Both DockWindow and MenuBarWindow switch from `WS_EX_LAYERED`/`UpdateLayeredWindow` to `WS_EX_NOREDIRECTIONBITMAP` + IDCompositionDevice visual tree.

---

### DEC-028 — Config Location
- **Date:** 2026-05-07
- **Session:** 015
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** `pinned_apps.json` lives next to the exe, which breaks when the exe is in a write-protected directory and gets lost on rebuilds/moves.
- **Options considered:**
  - Option A: `%APPDATA%\macOSWin\pinned_apps.json` — standard per-user app data location, survives exe moves, auto-created on first run. Migrate old next-to-exe config automatically.
  - Option B: Keep next-to-exe — no change, simple but fragile.
  - Option C: `%LOCALAPPDATA%\macOSWin\` — like A but doesn't roam.
- **Decision:** Option A — `%APPDATA%\macOSWin\pinned_apps.json` with auto-migration from old location.
- **Approved by:** Bala (via chat on 2026-05-07)
- **Notes:** On first run, if `%APPDATA%\macOSWin\pinned_apps.json` doesn't exist but the old next-to-exe config does, copy it over. All future reads/writes go to the new location.

---

### DEC-029 — Run at Windows Startup
- **Date:** 2026-05-07
- **Session:** 015
- **Raised by:** Claude Code
- **Status:** APPROVED
- **Context:** The overlay must be launched manually after every reboot. Auto-start would complete the macOS illusion.
- **Options considered:**
  - Option A: Registry key `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` with opt-in toggle in tray menu. Default off.
  - Option B: Startup folder shortcut (`.lnk` in `shell:startup`).
  - Option C: No auto-start.
- **Decision:** Option A — Registry `Run` key with a "Run at startup" toggle in the tray/quit menu. Default off.
- **Approved by:** Bala (via chat on 2026-05-07)
- **Notes:** Registry value name `macOSWin`, value = exe full path. Added/removed via `RegSetValueExW`/`RegDeleteValueW`. Toggle state read from registry on menu open. No admin rights required.

---

*Last updated: 2026-05-07 | Session 015*
