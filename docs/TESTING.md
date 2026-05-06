# Testing — macOS Windows Overlay

> Test strategy, test cases, and results per phase.
> Update results column as each test is executed.

---

## Testing Strategy

Since this is a native Windows UI app with no business logic layer, testing is primarily **manual functional testing** per phase, supplemented by:

- **Smoke tests** — app launches, windows appear, no crash
- **Functional tests** — each feature behaves as specified in REQUIREMENTS.md
- **Edge case tests** — unusual inputs, crash scenarios, recovery
- **Visual tests** — screenshots compared against macOS reference

Automated unit tests are gated behind `-DBUILD_TESTS=ON` (DEC-010, Session 003). Google Test runs via `ctest` and covers pure logic that can be tested without a window (Session 004: `ConfigManager`; planned: `AppLauncher` path validation, `SystemInfo` parsing). Win32 UI behavior stays as manual tests below.

---

## Phase 1 — Skeleton

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T1-001 | Launch app                                                   | App starts without error dialog                          | PASS   | Session 002, 2026-04-14 |
| T1-002 | Windows taskbar after launch                                 | Taskbar is hidden / auto-hidden                          | PASS   | Session 002, 2026-04-14 |
| T1-003 | Menu bar window visible                                      | Full-width bar visible at top of screen                  | PASS   | Session 002, 2026-04-14 |
| T1-004 | Dock window visible                                          | Centered dock visible at bottom of screen                | PASS   | Session 002, 2026-04-14 |
| T1-005 | Menu bar is always-on-top                                    | Maximizing any app does not cover the menu bar           | -      | Not yet tested — no quit UI means no safe way to alt-tab and verify |
| T1-006 | Dock is always-on-top                                        | Maximizing any app does not cover the Dock               | -      | Not yet tested — same reason as T1-005 |
| T1-007 | Quit via system tray                                         | App exits cleanly                                        | N/A    | No tray icon in Phase 1 — quit via Task Manager instead |
| T1-008 | Taskbar after quit (Task Manager kill)                       | Windows taskbar is restored after process is killed      | PASS   | Session 002, 2026-04-14 — RAII guard confirmed working |
| T1-009 | Taskbar after crash (force kill via Task Manager)            | Windows taskbar is restored after process is killed      | -      | Not explicitly force-killed; covered by T1-008 in practice |
| T1-010 | Re-launch after quit                                         | App launches again cleanly                               | -      | Not explicitly tested this session |

### Phase 1 Test Run Log

| Run | Date       | Tester | Build Config        | Outcome | Notes |
|-----|------------|--------|---------------------|---------|-------|
| 1   | 2026-04-14 | Bala   | Release x64, MSVC   | PASS    | Smoke test via "x64 Native Tools Command Prompt for VS 2022". `cmake -S . -B build -A x64` + `cmake --build build --config Release`. App launched, taskbar hidden, both windows appeared, taskbar restored on Task Manager kill. |

**Outstanding Phase 1 test gaps (carry into Phase 2 session):**
- T1-005, T1-006: always-on-top verification against a maximized window — easy to do once a quit mechanism exists (DEC-009).
- T1-009: deliberate crash/force-kill test — defer to Phase 2.
- T1-010: re-launch after clean quit — defer to Phase 2 (needs quit mechanism).

---

## Phase 2 — Dock Core

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T2-001 | Launch with empty `pinned_apps.json`                         | Dock shows no icons, no crash                            | PASS   | Session 004, 2026-04-15 — 120px placeholder bar visible |
| T2-002 | Launch with missing `pinned_apps.json`                       | File is auto-created, Dock shows empty                   | PASS   | Session 004, covered by gtest `LoadMissingFileCreatesEmptyConfig` |
| T2-003 | Launch with pre-populated config                             | Dock shows correct icons                                 | PASS   | Session 004 — 2 entries (Claude + File Explorer), both real icons rendered |
| T2-004 | Click pinned app icon (app not running)                      | App launches                                             | PASS   | Session 004 — Claude console + File Explorer window both launched |
| T2-005 | Click pinned app icon (app already running)                  | App window is brought to foreground                      | PASS   | Session 004 — re-click File Explorer brings existing window to front |
| T2-006 | Icon displays correctly for `.exe` file                      | App's real icon shown (not generic)                      | PASS   | Session 004 — explorer.exe rendered the folder icon |
| T2-007 | Icon displays correctly for `.lnk` shortcut                  | Target app's icon shown (not generic)                    | -      | Not exercised this session — no `.lnk` in test config. Code path covered by `SHGetFileInfoW` which handles both uniformly |
| T2-008 | Config file not corrupted after normal exit                  | JSON is valid, all entries intact                        | PASS   | Session 004 — Ctrl+Alt+Q quit, JSON unchanged |

### Phase 2 Test Run Log

| Run | Date       | Tester | Build Config      | Outcome | Notes |
|-----|------------|--------|-------------------|---------|-------|
| 1   | 2026-04-15 | Bala   | Release x64, MSVC | PASS    | Full manual smoke: config load → icon extraction → dock layout → click-to-launch → focus-existing. Initial build hit `LoadIconW`/`IDI_APPLICATION` LPSTR/LPCWSTR mismatch — fixed mid-session. First `explorer.exe` click focused the desktop (Progman) — fixed with `IsUserAppWindow` shell-class filter. Both fixes re-verified after rebuild. |

---

## Phase 3 — Interaction

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T3-001 | Drag `.exe` onto Dock                                        | Icon appears in Dock                                     | PASS   | Session 006, 2026-04-17 — Explorer drag works via CF_HDROP |
| T3-002 | Drag `.lnk` shortcut onto Dock                               | Icon appears using target app's icon (no arrow overlay)  | PASS   | Session 006, 2026-04-17 — resolved via IShellLink before SHGetFileInfoW |
| T3-003 | Drag non-app file onto Dock (e.g., `.txt`)                   | Dock flashes red briefly, no icon added                  | PASS   | Session 006, 2026-04-17 — 200ms red flash on reject |
| T3-004 | Drag multiple files onto Dock at once                        | All valid apps added; invalid files ignored              | -      | Not explicitly tested this session |
| T3-005 | Running indicator — app not running                          | No dot under icon                                        | PASS   | Session 006, 2026-04-17 |
| T3-006 | Running indicator — app is running                           | Dot appears under icon within 1.5s                       | PASS   | Session 006, 2026-04-17 |
| T3-007 | Running indicator updates after app closes                   | Dot disappears within 2 seconds                          | -      | Not explicitly tested |
| T3-008 | Pin order persisted after restart                            | Icons appear in same order as before                     | PASS   | Session 006, 2026-04-17 — config saved on every add/remove |
| T3-009 | Drag app from Start Menu                                     | Icon appears with correct app icon                       | PASS   | Session 006, 2026-04-17 — via Shell IDList Array + .lnk resolution |
| T3-010 | Drag UWP/Store app from Start Menu                           | Icon appears via shell:AppsFolder path                   | PASS   | Session 006, 2026-04-17 — IShellItemImageFactory icon extraction |
| T3-011 | Drag-off-dock to unpin                                       | Icon removed, config saved                               | PASS   | Session 006, 2026-04-17 — drag threshold 8px, release outside bounds removes |

### Phase 3 Test Run Log

| Run | Date       | Tester | Build Config      | Outcome | Notes |
|-----|------------|--------|-------------------|---------|-------|
| 1   | 2026-04-17 | Bala   | Release x64, MSVC | PASS    | Full manual smoke: Explorer drag, .lnk drag (no arrow), non-app reject (red flash), Start Menu Win32 drag, Start Menu UWP drag (Settings/Store), drag-off-dock unpin, running indicator dots. Three debug iterations for OLE init + DROPEFFECT + Start Menu app ID resolution. All verified green. |

**Outstanding Phase 3 test gaps:**
- T3-004: multi-file drag — not explicitly tested.
- T3-007: running indicator disappears after app closes — not explicitly tested.

---

## Phase 4 — Animation

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T4-001 | Hover cursor over Dock icon                                  | Icon smoothly scales up                                  | PASS   | Session 009, 2026-04-17 — verified by Bala |
| T4-002 | Move cursor across multiple icons                            | Fish-eye magnification follows cursor smoothly           | PASS   | Session 009, 2026-04-17 — verified by Bala |
| T4-003 | Move cursor off Dock                                         | Icons smoothly return to normal size                     | PASS   | Session 009, 2026-04-17 — timer polls GetCursorPos to detect genuine leave |
| T4-004 | Magnification at 60fps                                       | No stuttering observed during animation                  | PASS   | Session 009, 2026-04-17 — double-buffered OnPaint, immediate invalidate on MouseMove |
| T4-005 | Magnification max scale                                      | Icon directly under cursor scales to ~1.8x               | PASS   | Session 009, 2026-04-17 |
| T4-006 | Adjacent icon scaling                                        | Icons ~120px away from cursor show proportional scaling  | PASS   | Session 009, 2026-04-17 — cosine falloff within 120px radius |

### Phase 4 Test Run Log

| Run | Date       | Tester | Build Config      | Outcome | Notes |
|-----|------------|--------|-------------------|---------|-------|
| 1   | 2026-04-17 | Bala   | Release x64, MSVC | PASS    | Fish-eye magnification verified. Three bug fixes applied mid-session: (1) false WM_MOUSELEAVE oscillation fixed by moving cursor-outside check to TIMER_ANIMATE; (2) flicker fixed with double-buffered OnPaint + WM_ERASEBKGND suppression; (3) slow reaction fixed with immediate InvalidateRect in OnMouseMove. All Phase 3 features (drag-to-pin, unpin, running indicators, click-to-launch) verified as non-regressed. |

---

## Phase 5 — Menu Bar

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T5-001 | Clock displays correctly                                     | Shows current time in macOS format (Ddd D Mon HH:MM)     | PASS   | Session 011, 2026-04-18 — verified by Bala |
| T5-002 | Clock updates                                                | Display stays current (HH:MM, 2s refresh cadence)        | PASS   | Session 011, 2026-04-18 — seconds not shown; 2s tick is imperceptible |
| T5-003 | Battery on plugged-in laptop                                 | Shows charging bolt + percentage                         | PASS   | Session 011, 2026-04-18 — GDI+ vector glyph with charging bolt |
| T5-004 | Battery on unplugged laptop                                  | Shows discharge icon + percentage                        | -      | Not explicitly tested this session |
| T5-005 | Battery on desktop (no battery)                              | Battery widget shows AC/no-battery state                 | -      | Not explicitly tested |
| T5-006 | Volume indicator                                             | Reflects current system volume level                     | PASS   | Session 011, 2026-04-18 — speaker + sound-wave arcs rendered |
| T5-007 | Wi-Fi connected                                              | Shows Wi-Fi arc glyph + signal strength                  | PASS   | Session 011, 2026-04-18 — concentric arc glyph anti-aliased and DPI-sharp |
| T5-008 | Wi-Fi disconnected                                           | Shows disconnected state                                 | -      | Not explicitly tested |
| T5-009 | Active app name — switch between apps                        | Left of center in menu bar updates to show current app   | PASS   | Session 011, 2026-04-18 — switches in real time on alt-tab |
| T5-010 | Active app name — desktop is focused                         | Shows "Dock"                                             | PASS   | Session 011, 2026-04-18 — Progman/WorkerW filter → empty → display "Dock" |
| T5-011 | Media controls — Prev / Play-Pause / Next buttons            | Buttons send correct media key events                    | PASS   | Session 011, 2026-04-18 — verified with Spotify and browser |
| T5-012 | Media controls — play state toggles glyph                    | Glyph shows ▶ when paused, ⏸ when playing                | PASS   | Session 011, 2026-04-18 — SMTC atomic drives correct glyph automatically |
| T5-013 | DPI sharpness                                                | All widgets render crisp at system DPI (no bitmap stretch) | PASS | Session 011, 2026-04-18 — DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; all coords scaled |
| T5-014 | Translucent dock pill                                        | Rounded pill background at ~60% alpha behind icons       | PASS   | Session 011, 2026-04-18 — UpdateLayeredWindow AC_SRC_ALPHA confirmed sharp |

### Phase 5 Test Run Log

| Run | Date       | Tester | Build Config      | Outcome | Notes |
|-----|------------|--------|-------------------|---------|-------|
| 1   | 2026-04-18 | Bala   | Release x64, MSVC | PASS    | Session 011 — clock, battery (plugged), volume, Wi-Fi all rendered. Active app name updates on alt-tab. Desktop focus shows "Dock". Media controls verified with Spotify and browser; SMTC play-state glyph toggled correctly. DPI-sharp rendering confirmed. Translucent dock pill visible. No flicker on 2s timer tick. Ctrl+Alt+Q exits cleanly. |

---

## Phase 6 — Polish

| ID     | Test Case                                                    | Expected Result                                          | Result | Notes |
|--------|--------------------------------------------------------------|----------------------------------------------------------|--------|-------|
| T6-001 | Acrylic/blur on Dock                                         | Dock has frosted glass background                        | -      |       |
| T6-002 | Acrylic/blur on Menu Bar                                     | Menu bar has frosted glass background                    | -      |       |
| T6-003 | Dock entrance animation on launch                            | Dock slides up from bottom on startup                    | -      |       |
| T6-004 | Screen resolution change                                     | Dock and menu bar reposition correctly                   | -      |       |
| T6-005 | Windows 10 compatibility                                     | App runs on Windows 10 1903+                             | -      |       |
| T6-006 | Windows 11 compatibility                                     | App runs on Windows 11                                   | -      |       |

---

## Known Issues

| ID    | Phase | Description                                                                                      | Status | Workaround |
|-------|-------|--------------------------------------------------------------------------------------------------|--------|------------|
| KI-001 | 3–5  | On first launch, windows can extend behind the menu bar; resolves after minimise/restore          | Fixed (Session 007) | AppBarManager now posts WM_SETTINGCHANGE directly to existing maximized windows |
| KI-002 | 3    | Settings UWP icon looks incorrect — doesn't match the real Windows Settings gear icon            | Fixed (Session 008) | Factory tile path (no SIIGBF_ICONONLY) returns correct colored tile |
| KI-003 | 3    | Dock icons appear too large and pixelated                                                         | Fixed (Session 008) | GDI+ bicubic rendering from JUMBO 256×256 source |

---

## Automated Tests (ctest)

| ID     | Test Case                                                | Result | Notes |
|--------|----------------------------------------------------------|--------|-------|
| A-001  | `Smoke.PipelineIsAlive`                                  | PASS   | Session 003, 2026-04-14 |
| A-002  | `ConfigManagerTest.LoadMissingFileReturnsEmpty`          | PASS   | Session 005, 2026-04-15 |
| A-003  | `ConfigManagerTest.LoadMissingFileCreatesEmptyConfig`    | PASS   | Session 005, 2026-04-15 |
| A-004  | `ConfigManagerTest.SaveLoadRoundTrip`                    | PASS   | Session 005, 2026-04-15 |
| A-005  | `ConfigManagerTest.LoadMalformedJsonReturnsEmpty`        | PASS   | Session 005, 2026-04-15 |
| A-006  | `ConfigManagerTest.LoadEntryWithEmptyPathIsSkipped`      | PASS   | Session 005, 2026-04-15 |
| A-007  | `ConfigManagerTest.UnicodeRoundTrip`                     | PASS   | Session 005, 2026-04-15 |
| A-008  | `ConfigManagerTest.SaveEmptyListProducesEmptyArray`      | PASS   | Session 005, 2026-04-15 |
| A-009  | `DropValidator.AcceptsExeFile`                           | PASS   | Session 006, 2026-04-17 |
| A-010  | `DropValidator.AcceptsLnkFile`                           | PASS   | Session 006, 2026-04-17 |
| A-011  | `DropValidator.AcceptsUppercaseExtension`                | PASS   | Session 006, 2026-04-17 |
| A-012  | `DropValidator.AcceptsMixedCaseExtension`                | PASS   | Session 006, 2026-04-17 |
| A-013  | `DropValidator.RejectsTxtFile`                           | PASS   | Session 006, 2026-04-17 |
| A-014  | `DropValidator.RejectsBatFile`                           | PASS   | Session 006, 2026-04-17 |
| A-015  | `DropValidator.RejectsCmdFile`                           | PASS   | Session 006, 2026-04-17 |
| A-016  | `DropValidator.RejectsUrlFile`                           | PASS   | Session 006, 2026-04-17 |
| A-017  | `DropValidator.RejectsNoExtension`                       | PASS   | Session 006, 2026-04-17 |
| A-018  | `DropValidator.RejectsEmptyPath`                         | PASS   | Session 006, 2026-04-17 |
| A-019  | `DropValidator.RejectsDirectoryLookingPath`              | PASS   | Session 006, 2026-04-17 |
| A-020  | `DropValidator.ExtractsNameFromExePath`                  | PASS   | Session 006, 2026-04-17 |
| A-021  | `DropValidator.ExtractsNameFromLnkPath`                  | PASS   | Session 006, 2026-04-17 |
| A-022  | `DropValidator.ExtractsNameWithSpaces`                   | PASS   | Session 006, 2026-04-17 |
| A-023  | `DropValidator.ExtractsNameFromBareName`                 | PASS   | Session 006, 2026-04-17 |
| A-024  | `DropValidator.ExtractsNameNoExtension`                  | PASS   | Session 006, 2026-04-17 |
| A-025  | `DropValidator.ExtractsNameUnicode`                      | PASS   | Session 006, 2026-04-17 |

Run: `ctest --test-dir build -C Release --output-on-failure` (requires configure with `-DBUILD_TESTS=ON`).

### Automated Tests Run Log

| Run | Date       | Tester | Build Config      | Outcome       | Notes |
|-----|------------|--------|-------------------|---------------|-------|
| 1   | 2026-04-14 | Bala   | Release x64, MSVC | 1/1 PASS      | Session 003 — smoke-only canary. |
| 2   | 2026-04-15 | Bala   | Release x64, MSVC | 8/8 PASS      | Session 005 — `Smoke.PipelineIsAlive` + 7 `ConfigManagerTest` cases. Formal verification of Phase 2 unit suite. |
| 3   | 2026-04-17 | Bala   | Release x64, MSVC | 25/25 PASS    | Session 006 — all prior tests + 17 `DropValidator` cases. Phase 3 unit suite verified green. |

---

*Last updated: 2026-04-18 | Session 012 — Phase 5 tests verified green (v0.5.0)*
