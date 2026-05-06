// AppLauncher.cpp
// App launch and focus logic.

#include "AppLauncher.h"
#include <shellapi.h>
#include <psapi.h>
#include <algorithm>

struct FindWindowData {
    std::wstring exeName;
    HWND         result = nullptr;
};

// Returns true if hwnd looks like a user-facing top-level application window
// (as opposed to the desktop, taskbar, tray, or other shell background windows).
//
// This matters for explorer.exe specifically: it hosts many windows besides
// File Explorer — Progman/WorkerW (desktop), Shell_TrayWnd (taskbar),
// NotifyIconOverflowWindow (tray overflow), etc. Without this filter,
// LaunchOrFocus("explorer.exe") finds the desktop first and "focuses" it,
// which looks like nothing happened to the user.
static bool IsUserAppWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd)) return false;

    // Owned windows (dialogs, tooltips) aren't top-level app windows.
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;

    // Tool windows don't appear in alt-tab — treat the same way.
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;

    // A real app window has a non-empty title.
    if (GetWindowTextLengthW(hwnd) == 0) return false;

    // Reject known shell classes by name.
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, _countof(cls));
    static const wchar_t* const kShellClasses[] = {
        L"Progman",                  // desktop host
        L"WorkerW",                  // desktop wallpaper worker
        L"Shell_TrayWnd",            // taskbar
        L"Shell_SecondaryTrayWnd",   // secondary monitor taskbar
        L"NotifyIconOverflowWindow", // tray overflow flyout
        L"TrayNotifyWnd",            // notification area
    };
    for (const wchar_t* skip : kShellClasses)
        if (wcscmp(cls, skip) == 0) return false;

    return true;
}

static BOOL CALLBACK FindWindowCallback(HWND hwnd, LPARAM lParam)
{
    auto* data = reinterpret_cast<FindWindowData*>(lParam);
    if (!IsUserAppWindow(hwnd)) return TRUE;

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

    if (filename == data->exeName)
    {
        data->result = hwnd;
        return FALSE; // stop enumeration
    }
    return TRUE;
}

bool AppLauncher::LaunchOrFocus(const std::wstring& appPath)
{
    // Extract exe filename for running check
    size_t lastSlash = appPath.find_last_of(L"\\/");
    std::wstring exeName = (lastSlash != std::wstring::npos)
        ? appPath.substr(lastSlash + 1)
        : appPath;
    std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);

    // Check if already running
    HWND existing = FindMainWindowByExeName(exeName);
    if (existing)
    {
        ForceForeground(existing);
        return true;
    }

    // Launch the app
    HINSTANCE result = ShellExecuteW(
        nullptr, L"open", appPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    return reinterpret_cast<INT_PTR>(result) > 32;
}

HWND AppLauncher::FindMainWindowByExeName(const std::wstring& exeName)
{
    FindWindowData data;
    data.exeName = exeName;
    EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&data));
    return data.result;
}

void AppLauncher::ForceForeground(HWND hwnd)
{
    // Windows restricts SetForegroundWindow — workaround via AttachThreadInput
    DWORD fgThread  = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD myThread  = GetCurrentThreadId();

    if (fgThread != myThread)
        AttachThreadInput(myThread, fgThread, TRUE);

    SetForegroundWindow(hwnd);
    ShowWindow(hwnd, SW_RESTORE);

    if (fgThread != myThread)
        AttachThreadInput(myThread, fgThread, FALSE);
}
