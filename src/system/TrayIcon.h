// TrayIcon.h
// System tray notification icon with a right-click "Quit" menu.
// Owns a hidden message-only window to receive Shell_NotifyIcon callbacks.
// Provides the only user-facing path to exit the app cleanly (DEC-009).

#pragma once

#include <windows.h>
#include <shellapi.h>

class TrayIcon
{
public:
    explicit TrayIcon(HINSTANCE hInstance);
    ~TrayIcon();

    TrayIcon(const TrayIcon&)            = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Registers the tray icon. Returns false on failure; app should exit.
    bool Create();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Displays the right-click context menu at the cursor position.
    void ShowContextMenu();

    HINSTANCE        m_hInstance;
    HWND             m_hwnd;   // hidden message-only receiver window
    NOTIFYICONDATAW  m_nid;
    bool             m_iconAdded;
    bool             m_hotkeyRegistered;
};
