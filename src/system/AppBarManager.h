// AppBarManager.h
// Reserves screen space for the Dock (bottom) and MenuBar (top) by shrinking
// the Windows work area via SystemParametersInfo(SPI_SETWORKAREA).
//
// DEC-013 originally specified the AppBar API (SHAppBarMessage), but that
// stacks on top of the hidden taskbar's own AppBar registration, causing
// maximized windows to occupy only part of the screen. This revised approach
// directly sets the work area rect.
//
// Key detail: hiding Shell_TrayWnd causes Explorer to asynchronously reset
// the work area to full-screen. A sentinel window watches WM_SETTINGCHANGE
// and re-applies our work area whenever Explorer (or anyone else) overwrites it.
//
// RAII: constructor saves the original work area; Apply() sets the new one;
// destructor restores the original and destroys the sentinel.

#pragma once
#include <windows.h>

class AppBarManager
{
public:
    // topPx:    pixels reserved at the top    (MenuBar height)
    // bottomPx: pixels reserved at the bottom (Dock height + gap)
    AppBarManager(HINSTANCE hInstance, int topPx, int bottomPx);
    ~AppBarManager();

    AppBarManager(const AppBarManager&)            = delete;
    AppBarManager& operator=(const AppBarManager&) = delete;

    // Set the work area to exclude our reserved strips. Returns false if
    // the SystemParametersInfo call fails (non-fatal — overlay still runs).
    bool Apply();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Re-apply our desired work area if someone else changed it.
    void EnforceWorkArea();

    HINSTANCE m_hInstance;
    HWND      m_hwnd;             // hidden sentinel window for WM_SETTINGCHANGE
    RECT      m_originalWorkArea; // saved on construction, restored in destructor
    RECT      m_desiredWorkArea;  // what we want the work area to be
    int       m_topReserve;
    int       m_bottomReserve;
    bool      m_applied;          // true once Apply() succeeded
};
