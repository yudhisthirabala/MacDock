// CompositionHelper.h
// Window-rendering helper. After DEC-027's DComp path produced unrecoverable
// white-box artefacts on Win11 24H2 (multiple device-creation and GDI-alpha
// failures), this reverts to the pre-DEC-027 layered-window path:
//   WS_EX_LAYERED window + UpdateLayeredWindow + premultiplied DIB.
// Acrylic backdrop is sacrificed — icons render reliably.
//
// Public API (DCompWindow name kept for call-site compatibility):
//   Init(hwnd, w, h)  Resize(w, h)
//   HDC BeginDraw(POINT*)   EndDraw()   Commit()
//   Release()
//   members .w .h
//
// ApplySystemBackdrop and GetPrimaryMonitorSize remain unchanged.

#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace Composition {

// ─── System backdrop — Win11 22H2+ ───────────────────────────────────────────
// Preserved for reference. Has no effect on WS_EX_LAYERED windows.

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#  define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMSBT_TRANSIENTWINDOW
enum DWM_SYSTEMBACKDROP_TYPE : int {
    DWMSBT_AUTO            = 0,
    DWMSBT_NONE            = 1,
    DWMSBT_MAINWINDOW      = 2,
    DWMSBT_TRANSIENTWINDOW = 3,
    DWMSBT_TABBEDWINDOW    = 4,
};
#endif

inline bool ApplySystemBackdrop(HWND hwnd,
    DWM_SYSTEMBACKDROP_TYPE type = DWMSBT_TRANSIENTWINDOW)
{
    int v = static_cast<int>(type);
    return SUCCEEDED(DwmSetWindowAttribute(
        hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &v, sizeof(v)));
}

// ─── Primary monitor dimensions ───────────────────────────────────────────────

inline SIZE GetPrimaryMonitorSize()
{
    HMONITOR hMon = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(hMon, &mi))
    {
        return { mi.rcMonitor.right  - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top };
    }
    return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

// ─── Layered-window per-window rendering (replaces DComp path) ───────────────
// Name kept as DCompWindow for call-site compatibility with Phase-6 code that
// already references m_dcomp.Init/BeginDraw/EndDraw/Commit/Resize/Release.

struct DCompWindow
{
    int  w = 0, h = 0;

    // No shared device stack needed for layered windows. EnsureDevice is a
    // no-op kept for source compatibility.
    inline static bool EnsureDevice() { return true; }

    inline bool Init(HWND hwnd, int width, int height)
    {
        m_hwnd = hwnd;
        w = width; h = height;
        return EnsureDib(w, h);
    }

    inline bool Resize(int newW, int newH)
    {
        if (newW == w && newH == h) return true;
        w = newW; h = newH;
        return EnsureDib(w, h);
    }

    // Returns an HDC backed by a 32-bit top-down DIB, pre-cleared to transparent.
    // GDI+ writes alpha correctly into a DIB section.
    inline HDC BeginDraw(POINT* outOffset)
    {
        if (!m_memDC || !m_dibBits) return nullptr;
        std::memset(m_dibBits, 0, static_cast<size_t>(w) * h * 4);
        if (outOffset) *outOffset = { 0, 0 };
        return m_memDC;
    }

    // Premultiplies straight alpha and pushes the DIB to the layered window.
    inline void EndDraw()
    {
        if (!m_hwnd || !m_dibBits) return;

        // GDI+ writes straight alpha; UpdateLayeredWindow expects premultiplied.
        auto* px = static_cast<uint8_t*>(m_dibBits);
        const int count = w * h;
        for (int i = 0; i < count; ++i)
        {
            const uint8_t a = px[3];
            if (a == 0) { px[0] = px[1] = px[2] = 0; }
            else if (a < 255)
            {
                px[0] = static_cast<uint8_t>((px[0] * a + 127) / 255);
                px[1] = static_cast<uint8_t>((px[1] * a + 127) / 255);
                px[2] = static_cast<uint8_t>((px[2] * a + 127) / 255);
            }
            px += 4;
        }

        RECT wr; GetWindowRect(m_hwnd, &wr);
        POINT dst{ wr.left, wr.top };
        SIZE  sz { w, h };
        POINT src{ 0, 0 };
        BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        HDC screen = GetDC(nullptr);
        UpdateLayeredWindow(m_hwnd, screen, &dst, &sz, m_memDC, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, screen);
    }

    inline void Commit() { /* no-op — UpdateLayeredWindow already presents */ }

    // Re-push the existing bitmap at the window's current position.
    // Use after SetWindowPos to update the layered window without re-rendering.
    inline void Recommit()
    {
        if (!m_hwnd || !m_dibBits) return;
        RECT wr; GetWindowRect(m_hwnd, &wr);
        POINT dst{ wr.left, wr.top };
        SIZE  sz { w, h };
        POINT src{ 0, 0 };
        BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        HDC screen = GetDC(nullptr);
        UpdateLayeredWindow(m_hwnd, screen, &dst, &sz, m_memDC, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, screen);
    }

    inline void Release()
    {
        ReleaseDib();
        m_hwnd = nullptr;
        w = h = 0;
    }

private:
    HWND     m_hwnd     = nullptr;
    HDC      m_memDC    = nullptr;
    HBITMAP  m_dib      = nullptr;
    HBITMAP  m_oldBmp   = nullptr;
    void*    m_dibBits  = nullptr;
    int      m_dibW     = 0;
    int      m_dibH     = 0;

    inline bool EnsureDib(int width, int height)
    {
        if (m_memDC && m_dibW == width && m_dibH == height) return true;
        ReleaseDib();

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = width;
        bi.bmiHeader.biHeight      = -height;  // top-down
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC screen = GetDC(nullptr);
        m_memDC = CreateCompatibleDC(screen);
        m_dib   = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &m_dibBits, nullptr, 0);
        ReleaseDC(nullptr, screen);

        if (!m_memDC || !m_dib || !m_dibBits) { ReleaseDib(); return false; }

        m_oldBmp = static_cast<HBITMAP>(SelectObject(m_memDC, m_dib));
        m_dibW = width; m_dibH = height;
        return true;
    }

    inline void ReleaseDib()
    {
        if (m_memDC && m_oldBmp) { SelectObject(m_memDC, m_oldBmp); m_oldBmp = nullptr; }
        if (m_dib)   { DeleteObject(m_dib); m_dib = nullptr; }
        if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
        m_dibBits = nullptr;
        m_dibW = m_dibH = 0;
    }
};

// ─── Legacy blur-behind helpers (kept for reference) ─────────────────────────

enum AccentState : DWORD {
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3,
    ACCENT_ENABLE_ACRYLIC_BLURBEHIND  = 4,
};

struct AccentPolicy { AccentState accentState; DWORD accentFlags; DWORD gradientColor; DWORD animationId; };
struct WinCompAttrData { DWORD attribute; PVOID data; ULONG sizeOfData; };
using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WinCompAttrData*);

inline bool ApplyBlurBehind(HWND hwnd, DWORD abgrTint = 0x60181820)
{
    static auto fn = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!fn) return false;
    AccentPolicy policy{ ACCENT_ENABLE_BLURBEHIND, 2, abgrTint, 0 };
    WinCompAttrData data{ 19, &policy, sizeof(policy) };
    return fn(hwnd, &data) != FALSE;
}

inline bool EnableDwmBlur(HWND hwnd)
{
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE; bb.fEnable = TRUE; bb.hRgnBlur = nullptr;
    return SUCCEEDED(DwmEnableBlurBehindWindow(hwnd, &bb));
}

inline void RemoveBlurBehind(HWND hwnd)
{
    static auto fn = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!fn) return;
    AccentPolicy policy{ ACCENT_DISABLED, 0, 0, 0 };
    WinCompAttrData data{ 19, &policy, sizeof(policy) };
    fn(hwnd, &data);
}

} // namespace Composition
