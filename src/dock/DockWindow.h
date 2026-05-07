// DockWindow.h
// Main Dock container window — borderless, always-on-top, centered at screen bottom.
// Renders via UpdateLayeredWindow (per-pixel alpha) with GDI+ bicubic scaling.

#pragma once
#include <windows.h>
#include <oleidl.h>
#include <vector>
#include <memory>
#include "DockIcon.h"
#include "../system/CompositionHelper.h"

class DockWindow
{
public:
    explicit DockWindow(HINSTANCE hInstance);
    ~DockWindow();

    // Create and register the window class, then create the HWND
    bool Create();

    // Show the window (with DEC-023 slide-up entrance animation)
    void Show();

    // Recalculate dock position and size based on current icons
    void Reposition();

    // Add a new pinned app icon (called after drag-drop or config load)
    void AddIcon(const std::wstring& appPath, const std::wstring& appName);

    // Remove a pinned icon by index
    void RemoveIcon(int index);

    // Returns true if an icon with the given path (case-insensitive) is already pinned
    bool HasIcon(const std::wstring& appPath) const;

    // Called by DockDropTarget after a drop is processed (DEC-011)
    void OnDropComplete(int pinnedCount, int totalFiles);

    // Returns the underlying HWND
    HWND GetHWND() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void RenderDComp();      // actual GDI+ rendering into the DComp surface
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnTimer(WPARAM timerId);

    // Persist current icon list to pinned_apps.json
    void SaveConfig();

    // Flash the dock background red briefly to indicate a rejected drop (DEC-011)
    void FlashReject();

    // DPI scale factor (1.0 = 96 DPI). Computed once in Create().
    float m_dpiScale = 1.0f;
    int   S(int v) const { return static_cast<int>(v * m_dpiScale + 0.5f); }

    HINSTANCE    m_hInstance;
    HWND         m_hwnd;
    IDropTarget* m_dropTarget = nullptr;

    // DirectComposition rendering state (DEC-027)
    Composition::DCompWindow m_dcomp;

    std::vector<std::unique_ptr<DockIcon>> m_icons;

    // Drag-off-dock unpin state (DEC-012)
    bool  m_dragging   = false;
    int   m_dragIndex  = -1;
    POINT m_dragStart  = {};

    // Flash-red reject state (DEC-011)
    bool  m_flashActive = false;

    // Phase 4 magnification state (DEC-014, DEC-016)
    POINT m_cursorPos     = {-1,-1};
    bool  m_mouseTracking = false;

    // Entrance animation state (DEC-023)
    bool  m_entranceActive  = false;
    DWORD m_entranceStartMs = 0;
    int   m_targetY         = 0;

    // Layout constants (logical px, scaled by S() at use)
    static constexpr int   ICON_SIZE          = 28;
    static constexpr int   ICON_PADDING       = 20;
    static constexpr int   DOCK_HEIGHT        = 40;   // visible pill height
    static constexpr int   BOTTOM_GAP         = 2;
    static constexpr float MAGNIFY_MAX        = 1.8f;
    // Window is taller than the pill to give magnified icons room to grow upward.
    // Reduced from 72 to 60 vs. the layered-window version to shrink the acrylic
    // backdrop zone above the pill (DEC-027 tradeoff).
    static constexpr int   DOCK_WINDOW_HEIGHT = 60;
    // Y coordinate of the bottom edge of icon slots in client coords.
    static constexpr int   ICON_BOTTOM_Y      = DOCK_WINDOW_HEIGHT - 6;

    // Timer IDs
    static constexpr UINT_PTR TIMER_PROCESS_MONITOR = 1;
    static constexpr UINT_PTR TIMER_FLASH_REJECT    = 2;
    static constexpr UINT_PTR TIMER_ANIMATE         = 3;
    static constexpr UINT_PTR TIMER_ENTRANCE        = 4;

    // Running indicator dot dimensions
    static constexpr int DOT_RADIUS = 3;
    static constexpr int DOT_OFFSET = 4;
};
