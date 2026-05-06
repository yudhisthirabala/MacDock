// AppBarManager.cpp
// Reserves screen edges by shrinking the Windows work area.
// See AppBarManager.h for design rationale (DEC-013, revised).
//
// Key insight: hiding Shell_TrayWnd causes Explorer to asynchronously reset
// the work area. If we use SPIF_SENDCHANGE when setting our work area,
// Explorer receives WM_SETTINGCHANGE and fights back. Solution: set the work
// area SILENTLY (fWinIni = 0) so Explorer never hears about it. A sentinel
// window with a timer periodically checks and re-applies if anything
// overwrites our setting. On exit we restore with SPIF_SENDCHANGE so apps
// return to normal.

#include "AppBarManager.h"
#include "CompositionHelper.h"

static const wchar_t* SENTINEL_CLASS_NAME = L"macOSWin_WorkAreaSentinel";

// Directly resize any maximized non-shell windows to sit within our work area.
// PostMessage(WM_SETTINGCHANGE) is insufficient — most modern apps (Chrome, Edge,
// WinUI) don't re-snap in response to a direct message, only to a system broadcast.
// SetWindowPos bypasses that and physically moves the window to the correct bounds.
static BOOL CALLBACK ForceSnapMaximizedProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd) || !IsZoomed(hwnd)) return TRUE;

    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);

    // Skip Explorer shell windows — SetWindowPos on them would corrupt the shell
    static const wchar_t* shellClasses[] = {
        L"Shell_TrayWnd", L"Progman", L"WorkerW",
        L"Shell_SecondaryTrayWnd", L"NotifyIconOverflowWindow"
    };
    for (auto* sc : shellClasses)
        if (wcscmp(cls, sc) == 0) return TRUE;

    const RECT* wa = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd, nullptr,
                 wa->left, wa->top,
                 wa->right - wa->left, wa->bottom - wa->top,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    return TRUE;
}

// Timer ID and interval for periodic work-area enforcement.
static constexpr UINT_PTR ENFORCE_TIMER_ID = 5001;
static constexpr UINT     ENFORCE_TIMER_MS = 500; // check every 500ms

AppBarManager::AppBarManager(HINSTANCE hInstance, int topPx, int bottomPx)
    : m_hInstance(hInstance)
    , m_hwnd(nullptr)
    , m_originalWorkArea{}
    , m_desiredWorkArea{}
    , m_topReserve(topPx)
    , m_bottomReserve(bottomPx)
    , m_applied(false)
{
    // Save the current work area so we can restore it on exit.
    SystemParametersInfo(SPI_GETWORKAREA, 0, &m_originalWorkArea, 0);
}

AppBarManager::~AppBarManager()
{
    // Stop enforcing — destroy sentinel first so no more timer ticks
    if (m_hwnd)
    {
        KillTimer(m_hwnd, ENFORCE_TIMER_ID);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_applied)
    {
        // Restore the original work area WITH broadcast so all apps re-layout.
        SystemParametersInfo(SPI_SETWORKAREA, 0, &m_originalWorkArea, SPIF_SENDCHANGE);
        m_applied = false;
    }
}

bool AppBarManager::Apply()
{
    // DEC-025: scope work-area reservation to the primary monitor explicitly.
    const SIZE screen = Composition::GetPrimaryMonitorSize();

    // Compute desired work area: full screen minus our reserved strips.
    m_desiredWorkArea.left   = 0;
    m_desiredWorkArea.top    = m_topReserve;
    m_desiredWorkArea.right  = screen.cx;
    m_desiredWorkArea.bottom = screen.cy - m_bottomReserve;

    // Create the sentinel window FIRST so it can catch any async resets
    // that happen during or after our SPI_SETWORKAREA call.
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = AppBarManager::WndProc;
    wc.hInstance      = m_hInstance;
    wc.lpszClassName  = SENTINEL_CLASS_NAME;
    RegisterClassExW(&wc); // idempotent

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        SENTINEL_CLASS_NAME,
        L"",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, m_hInstance,
        this
    );

    // Set the work area SILENTLY — fWinIni = 0 means no WM_SETTINGCHANGE
    // broadcast. Explorer won't hear about it, so it won't fight back.
    // Any window maximized after this point will use our reduced rect.
    BOOL ok = SystemParametersInfo(SPI_SETWORKAREA, 0, &m_desiredWorkArea, 0);
    if (!ok) return false;
    m_applied = true;

    // Snap any already-maximized windows to the new work area immediately.
    // Without this, windows maximized before our app launched remain full-screen
    // until the user minimizes and restores them.
    EnumWindows(ForceSnapMaximizedProc, reinterpret_cast<LPARAM>(&m_desiredWorkArea));

    // Start a periodic timer that re-applies the work area if anything
    // (Explorer, display change, etc.) overwrites it.
    if (m_hwnd)
    {
        SetTimer(m_hwnd, ENFORCE_TIMER_ID, ENFORCE_TIMER_MS, nullptr);
    }

    return true;
}

void AppBarManager::EnforceWorkArea()
{
    if (!m_applied) return;

    // Check if someone changed the work area behind our back.
    RECT current = {};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &current, 0);

    if (current.left   != m_desiredWorkArea.left  ||
        current.top    != m_desiredWorkArea.top    ||
        current.right  != m_desiredWorkArea.right  ||
        current.bottom != m_desiredWorkArea.bottom)
    {
        // Re-apply SILENTLY — no broadcast, no fight with Explorer.
        SystemParametersInfo(SPI_SETWORKAREA, 0, &m_desiredWorkArea, 0);
        // Re-snap any maximized windows that expanded back during the reset.
        EnumWindows(ForceSnapMaximizedProc, reinterpret_cast<LPARAM>(&m_desiredWorkArea));
    }
}

LRESULT CALLBACK AppBarManager::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppBarManager* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<AppBarManager*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<AppBarManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_TIMER:
        if (wParam == ENFORCE_TIMER_ID)
        {
            self->EnforceWorkArea();
            return 0;
        }
        break;

    case WM_SETTINGCHANGE:
        // Someone changed a system setting — check if our work area was hit.
        self->EnforceWorkArea();
        return 0;

    case WM_DISPLAYCHANGE:
        // Monitor resolution changed — recalculate desired rect.
        {
            const SIZE screen = Composition::GetPrimaryMonitorSize();
            self->m_desiredWorkArea.left   = 0;
            self->m_desiredWorkArea.top    = self->m_topReserve;
            self->m_desiredWorkArea.right  = screen.cx;
            self->m_desiredWorkArea.bottom = screen.cy - self->m_bottomReserve;
            SystemParametersInfo(SPI_SETWORKAREA, 0, &self->m_desiredWorkArea, 0);
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
