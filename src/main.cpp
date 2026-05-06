// main.cpp
// Entry point for macOS Windows Overlay.
// Initializes OLE/COM, creates Dock and MenuBar windows, hides taskbar, runs message loop.

#include <windows.h>
#include <ole2.h>
#include <objbase.h>
#include <gdiplus.h>

#include "dock/DockWindow.h"
#include "menubar/MenuBarWindow.h"
#include "menubar/ActiveAppWatcher.h"
#include "system/TaskbarManager.h"
#include "system/TrayIcon.h"
#include "system/AppBarManager.h"

namespace {

// RAII guard: ensures the Windows taskbar is restored no matter how WinMain exits
// (normal return, exception, early-return from a window-creation failure).
// Without this guard, a crash or early return would leave the user's taskbar hidden.
class TaskbarHideGuard
{
public:
    TaskbarHideGuard()  { TaskbarManager::Hide(); }
    ~TaskbarHideGuard() { TaskbarManager::Restore(); }
    TaskbarHideGuard(const TaskbarHideGuard&)            = delete;
    TaskbarHideGuard& operator=(const TaskbarHideGuard&) = delete;
};

// RAII guard for OLE (superset of COM).
// OleInitialize sets up COM (STA) + OLE clipboard + drag-drop infrastructure.
// Required by RegisterDragDrop in DockWindow (Phase 3 drag-to-pin).
// Plain CoInitializeEx is NOT enough — RegisterDragDrop silently fails without
// the OLE subsystem, causing the "no drop" cursor on drag attempts.
class OleApartment
{
public:
    OleApartment() : m_ok(SUCCEEDED(OleInitialize(nullptr))) {}
    ~OleApartment() { if (m_ok) OleUninitialize(); }
    bool ok() const { return m_ok; }
    OleApartment(const OleApartment&)            = delete;
    OleApartment& operator=(const OleApartment&) = delete;
private:
    bool m_ok;
};

// RAII guard for GDI+. Required for high-quality bicubic icon scaling in
// DockWindow::OnPaint (Gdiplus::Graphics + InterpolationModeHighQualityBicubic).
// Without GDI+ initialization, Gdiplus::Bitmap/Graphics construction silently
// fails, leaving the dock empty.
class GdiplusApartment
{
public:
    GdiplusApartment() : m_token(0)
    {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&m_token, &input, nullptr);
    }
    ~GdiplusApartment() { if (m_token) Gdiplus::GdiplusShutdown(m_token); }
    GdiplusApartment(const GdiplusApartment&)            = delete;
    GdiplusApartment& operator=(const GdiplusApartment&) = delete;
private:
    ULONG_PTR m_token;
};

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Per-monitor DPI awareness — every child window (menu bar, dock) now
    // scales its own internal coordinates via GetDpiForSystem, so text and
    // glyphs render at native pixel density instead of being bitmap-stretched
    // (which is what produced the "everything pixelated" look).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    OleApartment     ole;           // must be first — sets up COM + OLE drag-drop
    GdiplusApartment gdip;          // GDI+ for bicubic-filtered icon rendering
    TaskbarHideGuard taskbarGuard;  // destructor restores taskbar on any exit path

    // Reserve screen work area so maximized apps stop at our bars (DEC-013).
    // Scale by DPI so the reservation matches the actual on-screen bar heights.
    static constexpr int MENUBAR_HEIGHT    = 38;
    static constexpr int DOCK_TOTAL_HEIGHT = 42;
    UINT sysDpi = GetDpiForSystem();
    AppBarManager workArea(hInstance,
                           MulDiv(MENUBAR_HEIGHT,    sysDpi, 96),
                           MulDiv(DOCK_TOTAL_HEIGHT, sysDpi, 96));
    workArea.Apply();

    // Create the top menu bar window
    MenuBarWindow menuBar(hInstance);
    if (!menuBar.Create()) return 1;
    menuBar.Show();

    // Create the bottom dock window
    DockWindow dock(hInstance);
    if (!dock.Create()) return 1;
    dock.Show();

    // Create the system tray icon — sole user-facing quit path (DEC-009).
    // Must succeed; without it the app can only be killed via Task Manager.
    TrayIcon tray(hInstance);
    if (!tray.Create()) return 1;

    // Foreground app watcher — updates menu bar name on focus changes (DEC-020).
    ActiveAppWatcher::Start([&menuBar](const std::wstring& name) {
        menuBar.SetActiveAppName(name);
    });

    // Main message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ActiveAppWatcher::Stop();
    return static_cast<int>(msg.wParam);
}
