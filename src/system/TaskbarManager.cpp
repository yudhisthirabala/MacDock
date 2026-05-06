// TaskbarManager.cpp
// Hides and restores the Windows taskbar using Win32.

#include "TaskbarManager.h"

HWND TaskbarManager::s_taskbarHwnd    = nullptr;
HWND TaskbarManager::s_startButtonHwnd = nullptr;
bool TaskbarManager::s_isHidden        = false;

bool TaskbarManager::Hide()
{
    if (s_isHidden) return true;

    s_taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!s_taskbarHwnd) return false;

    ShowWindow(s_taskbarHwnd, SW_HIDE);

    // Windows 11: also hide the secondary tray notification area
    HWND notifyWnd = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
    if (notifyWnd) ShowWindow(notifyWnd, SW_HIDE);

    s_isHidden = true;
    return true;
}

bool TaskbarManager::Restore()
{
    if (!s_isHidden) return true;

    if (s_taskbarHwnd)
    {
        ShowWindow(s_taskbarHwnd, SW_SHOW);
        s_taskbarHwnd = nullptr;
    }

    // Restore secondary tray if it exists
    HWND notifyWnd = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
    if (notifyWnd) ShowWindow(notifyWnd, SW_SHOW);

    s_isHidden = false;
    return true;
}
