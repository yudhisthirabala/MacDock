// ProcessMonitor.h
// Polls running processes to determine which pinned apps are currently open.
// Used to show/hide the running indicator dot under Dock icons.

#pragma once
#include <windows.h>
#include <string>
#include <unordered_set>

class ProcessMonitor
{
public:
    // Returns a set of exe filenames (lowercase, no path) for all running visible apps.
    // e.g. { L"chrome.exe", L"notepad.exe" }
    static std::unordered_set<std::wstring> GetRunningAppNames();

    // Returns true if a given exe filename is currently running.
    static bool IsRunning(const std::wstring& exeName);

private:
    // EnumWindows callback — collects visible top-level window process names
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
};
