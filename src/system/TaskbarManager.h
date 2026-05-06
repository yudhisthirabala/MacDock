// TaskbarManager.h
// Hides and restores the Windows taskbar (Shell_TrayWnd).
// Called on app launch (Hide) and app exit (Restore).

#pragma once
#include <windows.h>

class TaskbarManager
{
public:
    // Hide the Windows taskbar. Returns false if taskbar HWND not found.
    static bool Hide();

    // Restore the Windows taskbar to its original state.
    static bool Restore();

    // Returns true if the taskbar is currently hidden by us.
    static bool IsHidden() { return s_isHidden; }

private:
    static HWND s_taskbarHwnd;
    static HWND s_startButtonHwnd; // Windows 11 start button (separate HWND)
    static bool s_isHidden;
};
