// MenuBarWindow.h
// Full-width menu bar pinned to the top of the screen.
// Shows: media controls (left), active app name, Now-Playing text, system info (right).
// Phase 6 (DEC-027): WS_EX_NOREDIRECTIONBITMAP + DWMSBT_TRANSIENTWINDOW (real acrylic)
// + DirectComposition surface rendering.

#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include "SystemInfoBar.h"
#include "WidgetPopup.h"
#include "../system/CompositionHelper.h"

class MenuBarWindow
{
public:
    explicit MenuBarWindow(HINSTANCE hInstance);
    ~MenuBarWindow();

    // Create and register the window class, then create the HWND
    bool Create();

    // Show the window on screen
    void Show();

    // Update the displayed active app name (called by ActiveAppWatcher)
    void SetActiveAppName(const std::wstring& name);

    // Returns the underlying HWND
    HWND GetHWND() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void RenderDComp();  // actual GDI+ rendering into the DComp surface
    void OnTimer();
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonUp(int x, int y);

    HINSTANCE      m_hInstance;
    HWND           m_hwnd;
    std::wstring   m_activeAppName;
    SystemInfoData m_sysInfo;

    // DirectComposition rendering state (DEC-027)
    Composition::DCompWindow m_dcomp;

    // DPI scaling — computed in Create() from GetDpiForSystem.
    float m_dpiScale = 1.0f;

    // SMTC play state — atomic so the background worker can write
    // and the UI thread (OnPaint) can read without locking.
    std::atomic<bool> m_isPlaying{false};
    std::atomic<bool> m_smtcStop{false};
    std::thread       m_smtcThread;

    // Media control hit rects (updated each paint, read in OnLButtonUp).
    RECT m_prevRect = {};
    RECT m_playRect = {};
    RECT m_nextRect = {};

    // Widget hit rects (updated each paint, read in OnLButtonUp).
    WidgetHitRects m_widgetRects = {};

    // Widget hover popup
    std::unique_ptr<WidgetPopup> m_popup;
    WidgetType m_hoveredWidget = WidgetType::None;
    bool       m_mouseTracking = false;

    // Timer for delayed popup hide (allows moving from widget to popup)
    static constexpr UINT_PTR TIMER_POPUP_HIDE = 1002;
    static constexpr UINT     POPUP_HIDE_DELAY = 150;

    // Now-Playing track text (DEC-024). Written by SMTC worker; read by OnPaint.
    std::mutex   m_nowPlayingMutex;
    std::wstring m_nowPlayingText;

    // Entrance animation state
    bool  m_entranceActive  = false;
    DWORD m_entranceStartMs = 0;
    static constexpr UINT_PTR TIMER_ENTRANCE    = 1003;
    static constexpr UINT     ENTRANCE_DURATION = 250;

    // Layout constants — logical pixels, multiplied by m_dpiScale at draw time.
    static constexpr int BAR_HEIGHT = 38;
};
