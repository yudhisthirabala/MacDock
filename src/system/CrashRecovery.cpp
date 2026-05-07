// CrashRecovery.cpp
// Crash sentinel: a lock file in %APPDATA%\macOSWin\.running signals the
// taskbar is hidden. On startup, if the file exists, the previous instance
// crashed — restore the taskbar before proceeding. On clean exit, delete it.

#include "CrashRecovery.h"
#include "TaskbarManager.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

namespace {

std::wstring GetSentinelPath()
{
    wchar_t appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        return {};
    return std::wstring(appData) + L"\\macOSWin\\.running";
}

void RestoreTaskbarFromCrash()
{
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (taskbar && !IsWindowVisible(taskbar))
    {
        ShowWindow(taskbar, SW_SHOW);
    }
    HWND secondary = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
    if (secondary && !IsWindowVisible(secondary))
    {
        ShowWindow(secondary, SW_SHOW);
    }
}

LONG WINAPI CrashExceptionFilter(EXCEPTION_POINTERS*)
{
    RestoreTaskbarFromCrash();

    std::wstring sentinel = GetSentinelPath();
    if (!sentinel.empty())
        DeleteFileW(sentinel.c_str());

    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void CrashRecovery::OnStartup()
{
    std::wstring sentinel = GetSentinelPath();
    if (sentinel.empty()) return;

    if (GetFileAttributesW(sentinel.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        RestoreTaskbarFromCrash();
        DeleteFileW(sentinel.c_str());
    }

    CreateDirectoryW((std::wstring(sentinel.begin(), sentinel.begin() + sentinel.find_last_of(L'\\'))).c_str(), nullptr);
    HANDLE h = CreateFileW(sentinel.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
}

void CrashRecovery::OnCleanExit()
{
    std::wstring sentinel = GetSentinelPath();
    if (!sentinel.empty())
        DeleteFileW(sentinel.c_str());
}

void CrashRecovery::InstallExceptionHandler()
{
    SetUnhandledExceptionFilter(CrashExceptionFilter);
}
