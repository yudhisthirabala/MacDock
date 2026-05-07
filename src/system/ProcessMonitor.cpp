// ProcessMonitor.cpp
// EnumWindows-based running process detection.

#include "ProcessMonitor.h"
#include <psapi.h>
#include <algorithm>

// Apps to exclude from the "running but not pinned" list
static bool IsSystemProcess(const std::wstring& exeLower)
{
    static const wchar_t* skip[] = {
        L"explorer.exe", L"shellexperiencehost.exe", L"searchhost.exe",
        L"startmenuexperiencehost.exe", L"textinputhost.exe",
        L"applicationframehost.exe", L"systemsettingsbroker.exe",
        L"runtimebroker.exe", L"dwm.exe", L"csrss.exe",
        L"svchost.exe", L"taskmgr.exe", L"conhost.exe",
        L"searchui.exe", L"cortana.exe", L"lockapp.exe",
        L"widgetservice.exe", L"widgets.exe",
        L"macoswint.exe", L"macoswin.exe",
    };
    for (const auto* s : skip)
        if (exeLower == s) return true;
    return false;
}

std::unordered_set<std::wstring> ProcessMonitor::GetRunningAppNames()
{
    std::unordered_set<std::wstring> result;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&result));
    return result;
}

std::unordered_map<std::wstring, std::wstring> ProcessMonitor::GetRunningAppPaths()
{
    std::unordered_map<std::wstring, std::wstring> result;
    EnumWindows(EnumWindowsPathProc, reinterpret_cast<LPARAM>(&result));
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

    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) return TRUE;
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return TRUE;

    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH);
    CloseHandle(hProcess);

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

BOOL CALLBACK ProcessMonitor::EnumWindowsPathProc(HWND hwnd, LPARAM lParam)
{
    auto* result = reinterpret_cast<std::unordered_map<std::wstring, std::wstring>*>(lParam);

    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd)) return TRUE;
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return TRUE;

    // Skip owned windows (child dialogs, tooltips, etc.)
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return TRUE;

    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameExW(hProcess, nullptr, path, MAX_PATH);
    CloseHandle(hProcess);

    std::wstring fullPath(path);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    std::wstring filename = (lastSlash != std::wstring::npos)
        ? fullPath.substr(lastSlash + 1)
        : fullPath;
    std::transform(filename.begin(), filename.end(), filename.begin(), ::towlower);

    if (!filename.empty() && !IsSystemProcess(filename))
        result->emplace(filename, fullPath);

    return TRUE;
}
