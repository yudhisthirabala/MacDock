// ProcessMonitor.cpp
// EnumWindows-based running process detection.

#include "ProcessMonitor.h"
#include <psapi.h>
#include <algorithm>

std::unordered_set<std::wstring> ProcessMonitor::GetRunningAppNames()
{
    std::unordered_set<std::wstring> result;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool ProcessMonitor::IsRunning(const std::wstring& exeName)
{
    auto running = GetRunningAppNames();
    std::wstring lower = exeName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return running.count(lower) > 0;
}

BOOL CALLBACK ProcessMonitor::EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* result = reinterpret_cast<std::unordered_set<std::wstring>*>(lParam);

    // Consider visible OR minimized top-level windows with a title
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) return TRUE;
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return TRUE;

    // Get the process name
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return TRUE;

    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH);
    CloseHandle(hProcess);

    // Extract and lowercase the filename
    std::wstring fullPath(path);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    std::wstring filename = (lastSlash != std::wstring::npos)
        ? fullPath.substr(lastSlash + 1)
        : fullPath;
    std::transform(filename.begin(), filename.end(), filename.begin(), ::towlower);

    if (!filename.empty())
        result->insert(filename);

    return TRUE;
}
