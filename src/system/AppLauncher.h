// AppLauncher.h
// Launches or focuses an application by its exe or lnk path.

#pragma once
#include <windows.h>
#include <string>

class AppLauncher
{
public:
    // Launch the app at the given path.
    // If the app is already running, brings its main window to the foreground instead.
    // Returns true on success.
    static bool LaunchOrFocus(const std::wstring& appPath);

private:
    // Finds the main window of a running process by its exe filename.
    // Returns nullptr if not found.
    static HWND FindMainWindowByExeName(const std::wstring& exeName);

    // Attempts to bring a window to the foreground (workaround for Windows focus restrictions).
    static void ForceForeground(HWND hwnd);
};
