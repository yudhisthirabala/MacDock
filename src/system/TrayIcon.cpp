// TrayIcon.cpp
// Implementation of the system tray icon + Quit menu (DEC-009).

#include "TrayIcon.h"

namespace {

constexpr const wchar_t* TRAY_CLASS_NAME = L"macOSWin_TrayReceiver";
constexpr const wchar_t* TRAY_TOOLTIP    = L"macOS Windows Overlay";

// Custom window message used by Shell_NotifyIcon to report tray events.
// Must be in the WM_APP range so it cannot collide with system messages.
constexpr UINT WM_TRAYICON = WM_APP + 1;

// Stable UID for this app's tray icon (arbitrary; must be unique within this process).
constexpr UINT TRAY_ICON_UID = 1;

// Context-menu command IDs.
constexpr UINT IDM_QUIT = 1001;

// Global hotkey ID (per-window; any unique int will do).
// Ctrl+Alt+Q — primary quit path (DEC-009 revised), necessary because
// `Shell_TrayWnd` is hidden in Phase 1 so the tray icon itself isn't clickable.
constexpr int HOTKEY_QUIT_ID = 1;

} // namespace

TrayIcon::TrayIcon(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwnd(nullptr), m_nid{},
      m_iconAdded(false), m_hotkeyRegistered(false)
{
}

TrayIcon::~TrayIcon()
{
    if (m_hotkeyRegistered)
    {
        UnregisterHotKey(m_hwnd, HOTKEY_QUIT_ID);
        m_hotkeyRegistered = false;
    }
    if (m_iconAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_iconAdded = false;
    }
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool TrayIcon::Create()
{
    // Register a minimal window class for the hidden receiver window.
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = TrayIcon::WndProc;
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = TRAY_CLASS_NAME;
    RegisterClassExW(&wc); // ignore failure — class may already be registered

    // Message-only window (HWND_MESSAGE) — never visible, never in z-order.
    m_hwnd = CreateWindowExW(
        0, TRAY_CLASS_NAME, L"", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, m_hInstance,
        this // for WM_NCCREATE
    );
    if (!m_hwnd) return false;

    // Populate NOTIFYICONDATA.
    m_nid.cbSize           = sizeof(m_nid);
    m_nid.hWnd             = m_hwnd;
    m_nid.uID              = TRAY_ICON_UID;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon            = LoadIcon(nullptr, IDI_APPLICATION); // placeholder until Phase 6
    lstrcpynW(m_nid.szTip, TRAY_TOOLTIP, sizeof(m_nid.szTip) / sizeof(m_nid.szTip[0]));

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) return false;
    m_iconAdded = true;

    // Register Ctrl+Alt+Q as the primary quit path (DEC-009 revised).
    // Failure here is non-fatal — tray icon still works as fallback if the
    // user has a tray visible (e.g. after refactoring taskbar-hide in Phase 6).
    // Note: MOD_NOREPEAT requires Win7+ and prevents auto-repeat firing.
    if (RegisterHotKey(m_hwnd, HOTKEY_QUIT_ID,
                       MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q'))
    {
        m_hotkeyRegistered = true;
    }

    return true;
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayIcon* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TrayIcon*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_TRAYICON:
        // LOWORD(lParam) is the notification event (WM_LBUTTONUP, WM_RBUTTONUP, etc.)
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
        {
            self->ShowContextMenu();
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_QUIT)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_HOTKEY:
        // Only ID we register is HOTKEY_QUIT_ID; any WM_HOTKEY means quit.
        if (static_cast<int>(wParam) == HOTKEY_QUIT_ID)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void TrayIcon::ShowContextMenu()
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit macOS Overlay");

    // Required so the menu disappears when the user clicks outside it.
    SetForegroundWindow(m_hwnd);

    TrackPopupMenu(menu,
                   TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, m_hwnd, nullptr);

    DestroyMenu(menu);
}
