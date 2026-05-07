// CrashRecovery.h
// Restores the Windows taskbar if the previous instance crashed without cleanup.
// Uses a lock file in %APPDATA%\macOSWin\ as a crash sentinel.

#pragma once

class CrashRecovery
{
public:
    // Call before hiding the taskbar. Checks for stale sentinel and restores
    // taskbar if a previous crash left it hidden. Then creates a new sentinel.
    static void OnStartup();

    // Call on clean exit (after taskbar is restored). Removes the sentinel.
    static void OnCleanExit();

    // Install unhandled exception filter that restores taskbar before crashing.
    static void InstallExceptionHandler();
};
