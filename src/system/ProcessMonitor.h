// ProcessMonitor.h
// Polls running processes to determine which pinned apps are currently open.
// Used to show/hide the running indicator dot under Dock icons.

#pragma once
#include <windows.h>
#include <string>
#include <unordered_set>
#include <unordered_map>

class ProcessMonitor
{
public:
    // Returns a set of exe filenames (lowercase, no path) for all running visible apps.
    static std::unordered_set<std::wstring> GetRunningAppNames();

    // Returns a map of exe filename (lowercase) -> full exe path for running apps.
    static std::unordered_map<std::wstring, std::wstring> GetRunningAppPaths();

    // Returns true if a given exe filename is currently running.
    static bool IsRunning(const std::wstring& exeName);

private:
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    static BOOL CALLBACK EnumWindowsPathProc(HWND hwnd, LPARAM lParam);
};
