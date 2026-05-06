// ActiveAppWatcher.h
// Monitors the active foreground window using Win32 SetWinEventHook.
// Fires a callback when the user switches applications.

#pragma once
#include <windows.h>
#include <string>
#include <functional>

class ActiveAppWatcher
{
public:
    using Callback = std::function<void(const std::wstring& appName)>;

    // Start watching for foreground window changes
    // callback is called with the app name whenever the active window changes
    static bool Start(Callback callback);

    // Stop watching and remove the event hook
    static void Stop();

private:
    // Win32 WinEvent callback — called by the OS on EVENT_SYSTEM_FOREGROUND
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime);

    // Extracts a friendly app name from an HWND
    static std::wstring GetAppNameFromHwnd(HWND hwnd);

    static HWINEVENTHOOK s_hook;
    static Callback      s_callback;
};
