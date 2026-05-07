// MenuBarWindow.cpp
// Top menu bar: media-control cluster · active app name · Now-Playing text · system info.
// Phase 6 (DEC-027): DirectComposition rendering replaces WS_EX_LAYERED / BitBlt.
// DWMSBT_TRANSIENTWINDOW provides real OS acrylic on Win11 22H2+.

#include "MenuBarWindow.h"
#include "SystemInfoBar.h"
#include "../system/CompositionHelper.h"

#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>
#include <chrono>

#if __has_include(<winrt/Windows.Media.Control.h>)
  #define HAS_SMTC 1
  #include <winrt/Windows.Foundation.h>
  #include <winrt/Windows.Media.Control.h>
#else
  #define HAS_SMTC 0
#endif

using namespace Gdiplus;

static const wchar_t* MENUBAR_CLASS_NAME = L"macOSWin_MenuBar";
static constexpr UINT TIMER_TICK_ID      = 1001;
static constexpr UINT TIMER_TICK_MS      = 1000;

// Logical-pixel layout constants (multiplied by m_dpiScale at draw time).
static constexpr int LEFT_PAD         = 12;
static constexpr int BTN_W            = 28;
static constexpr int BTN_H            = 22;
static constexpr int BTN_GAP          = 2;
static constexpr int AFTER_MEDIA_GAP  = 16;
static constexpr int APP_NAME_FONT_PX = 13;
static constexpr int GLYPH_FONT_PX    = 14;

// Bar colours — semi-transparent so the DWMSBT_TRANSIENTWINDOW acrylic (blurred
// desktop) shows through as frosted glass behind our tinted overlay.
// Using CompositingModeSourceCopy, GDI+ writes these values directly into the
// premultiplied BGRA DComp surface. Dark colours at 70-80% alpha; the small
// premult error (not pre-dividing R/G/B by alpha) is visually imperceptible
// at these near-black values and matches the existing dock pill behaviour.
static const Color kBarTop(200, 28, 28, 34);   // ~78% opaque dark grey top
static const Color kBarMid(180, 22, 22, 26);   // ~71% opaque dark grey mid
static const Color kBarBot(160, 16, 16, 20);   // ~63% opaque dark grey bottom
static const Color kFg(255, 245, 245, 247);
static const Color kBtnHover(56, 255, 255, 255);
static const Color kTrackFg(200, 180, 180, 185);

// ─── Constructor / Destructor ─────────────────────────────────────────────────

MenuBarWindow::MenuBarWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwnd(nullptr), m_activeAppName(L"Dock"),
      m_flyout(hInstance)
{
    m_sysInfo.battery       = -1;
    m_sysInfo.charging      = false;
    m_sysInfo.volume        = 0;
    m_sysInfo.wifiConnected = false;
    m_sysInfo.wifiQuality   = 0;
    m_sysInfo.clock         = L"";
}

MenuBarWindow::~MenuBarWindow()
{
    m_smtcStop.store(true);
    if (m_smtcThread.joinable()) m_smtcThread.join();
    m_dcomp.Release();
    if (m_hwnd)
    {
        KillTimer(m_hwnd, TIMER_TICK_ID);
        DestroyWindow(m_hwnd);
    }
}

// ─── Create ──────────────────────────────────────────────────────────────────

bool MenuBarWindow::Create()
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MenuBarWindow::WndProc;
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = MENUBAR_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_HAND);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    UINT dpi = GetDpiForSystem();
    m_dpiScale = dpi / 96.0f;

    SIZE mon    = Composition::GetPrimaryMonitorSize();
    int screenW = mon.cx;
    int barH    = static_cast<int>(BAR_HEIGHT * m_dpiScale + 0.5f);

    // DEC-027 reverted to WS_EX_LAYERED — see CompositionHelper.h notes.
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        MENUBAR_CLASS_NAME, L"MenuBar",
        WS_POPUP,
        0, 0, screenW, barH,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) return false;

    // ApplySystemBackdrop disabled — incompatible with WS_EX_LAYERED.

    if (!m_dcomp.Init(m_hwnd, screenW, barH))
        return false;

    m_sysInfo = SystemInfoBar::Fetch();
    SetTimer(m_hwnd, TIMER_TICK_ID, TIMER_TICK_MS, nullptr);

#if HAS_SMTC
    m_smtcThread = std::thread([this]() {
        try { winrt::init_apartment(winrt::apartment_type::multi_threaded); } catch(...) {}
        while (!m_smtcStop.load())
        {
            bool         playing   = false;
            std::wstring nowPlaying;
            try {
                using namespace winrt::Windows::Media::Control;
                auto mgr  = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                auto sess = mgr.GetCurrentSession();
                if (sess) {
                    auto info = sess.GetPlaybackInfo();
                    playing = (info.PlaybackStatus() ==
                        GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                    try {
                        auto props = sess.TryGetMediaPropertiesAsync().get();
                        if (props) {
                            auto title  = props.Title();
                            auto artist = props.Artist();
                            if (!title.empty()) {
                                nowPlaying = title.c_str();
                                if (!artist.empty())
                                    nowPlaying += std::wstring(L" \u2014 ") + artist.c_str();
                            }
                        }
                    } catch (...) {}
                }
            } catch (...) { playing = false; }

            bool repaint = false;
            if (playing != m_isPlaying.load()) {
                m_isPlaying.store(playing);
                repaint = true;
            }
            {
                std::lock_guard<std::mutex> lock(m_nowPlayingMutex);
                if (nowPlaying != m_nowPlayingText) {
                    m_nowPlayingText = nowPlaying;
                    repaint = true;
                }
            }
            if (repaint && m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);

            for (int i = 0; i < 20 && !m_smtcStop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
#endif
    return true;
}

// ─── Show ────────────────────────────────────────────────────────────────────

void MenuBarWindow::Show()
{
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    // Render initial content immediately so the bar is visible on first frame.
    RenderDComp();
}

// ─── SetActiveAppName ────────────────────────────────────────────────────────

void MenuBarWindow::SetActiveAppName(const std::wstring& name)
{
    std::wstring display = name.empty() ? L"Dock" : name;
    if (display == m_activeAppName) return;
    m_activeAppName = display;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

LRESULT CALLBACK MenuBarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MenuBarWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MenuBarWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<MenuBarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        self->RenderDComp();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        self->OnTimer();
        return 0;

    case WM_MOUSEMOVE:
    {
        if (!self->m_trackingMouse)
        {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            self->m_trackingMouse = true;
        }
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        self->OnMouseMove(mx, my);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE:
        self->m_trackingMouse = false;
        self->OnMouseLeave();
        return 0;

    case WM_LBUTTONUP:
        self->OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_DISPLAYCHANGE:
        self->Show(); // re-render after display configuration change
        return 0;

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ─── Helper functions (file-local) ────────────────────────────────────────────

namespace {

void SendMediaKey(WORD vk)
{
    INPUT inputs[2] = {};
    inputs[0].type     = INPUT_KEYBOARD;
    inputs[0].ki.wVk   = vk;
    inputs[1].type     = INPUT_KEYBOARD;
    inputs[1].ki.wVk   = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void FillRoundedRect(Graphics& g, const Color& c,
                     float x, float y, float w, float h, float r)
{
    GraphicsPath p;
    p.AddArc(x,             y,             r * 2, r * 2, 180.0f, 90.0f);
    p.AddArc(x + w - r * 2, y,             r * 2, r * 2, 270.0f, 90.0f);
    p.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2,   0.0f, 90.0f);
    p.AddArc(x,             y + h - r * 2, r * 2, r * 2,  90.0f, 90.0f);
    p.CloseFigure();
    SolidBrush b(c);
    g.FillPath(&b, &p);
}

void DrawGlyph(Graphics& g, const RECT& r, wchar_t glyph, float fontPx)
{
    FontFamily fluent(L"Segoe Fluent Icons");
    FontFamily mdl2(L"Segoe MDL2 Assets");
    FontFamily* fam = (fluent.GetLastStatus() == Ok) ? &fluent : &mdl2;

    Font font(fam, fontPx, FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);

    RectF box(static_cast<REAL>(r.left), static_cast<REAL>(r.top),
              static_cast<REAL>(r.right - r.left),
              static_cast<REAL>(r.bottom - r.top));

    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.DrawString(&glyph, 1, &font, box, &sf, &fg);
}

bool PointInRect(int x, int y, const RECT& r)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

} // anonymous namespace

// ─── OnPaint / RenderDComp ────────────────────────────────────────────────────

void MenuBarWindow::OnPaint()
{
    ValidateRect(m_hwnd, nullptr);
    RenderDComp();
}

void MenuBarWindow::RenderDComp()
{
    POINT offset;
    HDC hdc = m_dcomp.BeginDraw(&offset);
    if (!hdc) return;

    const int w = m_dcomp.w;
    const int h = m_dcomp.h;

    {   // inner scope: Graphics destroyed before EndDraw
        const float s    = m_dpiScale;
        auto ipx = [s](float v) { return static_cast<int>(v * s + 0.5f); };
        auto px  = [s](float v) { return v * s; };

        Graphics g(hdc);
        g.TranslateTransform(static_cast<REAL>(offset.x),
                             static_cast<REAL>(offset.y));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);

        // Clear surface to transparent first (ensures areas not painted are glass).
        g.SetCompositingMode(CompositingModeSourceCopy);
        g.Clear(Color(0, 0, 0, 0));

        // Semi-transparent gradient: DWMSBT acrylic shows through where alpha < 255,
        // giving the frosted-glass-over-dark-tint macOS menu bar appearance.
        // SourceCopy writes our Color(alpha, R, G, B) directly into the premultiplied
        // DComp surface — the DWM compositor blends it with the acrylic behind.
        LinearGradientBrush bgTop(Point(0, 0),     Point(0, h / 2), kBarTop, kBarMid);
        LinearGradientBrush bgBot(Point(0, h / 2), Point(0, h),     kBarMid, kBarBot);
        g.FillRectangle(&bgTop, 0, 0,     w, h / 2);
        g.FillRectangle(&bgBot, 0, h / 2, w, h - h / 2);

        g.SetCompositingMode(CompositingModeSourceOver);

        Pen hilite(Color(36, 255, 255, 255), 1.0f);
        g.DrawLine(&hilite, 0, 0, w, 0);
        Pen edge(Color(120, 0, 0, 0), 1.0f);
        g.DrawLine(&edge, 0, h - 1, w, h - 1);

        // Media button rects.
        int btnW = ipx(BTN_W);
        int btnH = ipx(BTN_H);
        int gap  = ipx(BTN_GAP);
        int lp   = ipx(LEFT_PAD);
        int yTop = (h - btnH) / 2;

        m_prevRect = { lp,                     yTop, lp + btnW,                     yTop + btnH };
        m_playRect = { m_prevRect.right + gap,  yTop, m_prevRect.right + gap + btnW, yTop + btnH };
        m_nextRect = { m_playRect.right + gap,  yTop, m_playRect.right + gap + btnW, yTop + btnH };

        POINT cursor; GetCursorPos(&cursor);
        ScreenToClient(m_hwnd, &cursor);
        auto drawHover = [&](const RECT& r) {
            if (PointInRect(cursor.x, cursor.y, r))
                FillRoundedRect(g, kBtnHover,
                                static_cast<float>(r.left),  static_cast<float>(r.top),
                                static_cast<float>(r.right - r.left),
                                static_cast<float>(r.bottom - r.top), px(5.0f));
        };
        drawHover(m_prevRect);
        drawHover(m_playRect);
        drawHover(m_nextRect);

        const wchar_t kPrev  = L'\uE892';
        const wchar_t kPlay  = L'\uE768';
        const wchar_t kPause = L'\uE769';
        const wchar_t kNext  = L'\uE893';
        wchar_t mid = m_isPlaying.load() ? kPause : kPlay;

        float glyphPx = px(GLYPH_FONT_PX);
        DrawGlyph(g, m_prevRect, kPrev, glyphPx);
        DrawGlyph(g, m_playRect, mid,   glyphPx);
        DrawGlyph(g, m_nextRect, kNext, glyphPx);

        FontFamily family(L"Segoe UI");
        Font       nameFont(&family, px(APP_NAME_FONT_PX), FontStyleBold, UnitPixel);
        SolidBrush fg(kFg);

        float nameX = static_cast<float>(m_nextRect.right) + px(AFTER_MEDIA_GAP);
        RectF nameBounds;
        g.MeasureString(m_activeAppName.c_str(), -1, &nameFont, PointF(0, 0), &nameBounds);
        float nameY = static_cast<float>(h) / 2.0f - nameBounds.Height / 2.0f;
        g.DrawString(m_activeAppName.c_str(), -1, &nameFont, PointF(nameX, nameY), &fg);

        std::wstring track;
        {
            std::lock_guard<std::mutex> lock(m_nowPlayingMutex);
            track = m_nowPlayingText;
        }
        if (!track.empty())
        {
            Font       trackFont(&family, px(11.0f), FontStyleRegular, UnitPixel);
            SolidBrush trackBrush(kTrackFg);
            float trackX = nameX + nameBounds.Width + px(14.0f);
            float maxW   = px(220.0f);
            RectF availBox(trackX, 0.0f, maxW, static_cast<REAL>(h));
            StringFormat sf;
            sf.SetAlignment(StringAlignmentNear);
            sf.SetLineAlignment(StringAlignmentCenter);
            sf.SetTrimming(StringTrimmingEllipsisCharacter);
            sf.SetFormatFlags(StringFormatFlagsNoWrap);
            g.DrawString(track.c_str(), -1, &trackFont, availBox, &sf, &trackBrush);
        }

        // Right-side widget cluster (clock, battery, volume, Wi-Fi).
        RECT rc = { 0, 0, w, h };
        SystemInfoBar::Render(hdc, rc, m_sysInfo, &m_widgetRects);
    }  // g destroyed here — required before EndDraw

    m_dcomp.EndDraw();
    m_dcomp.Commit();
}

// ─── Timer / click ────────────────────────────────────────────────────────────

void MenuBarWindow::OnTimer()
{
    m_sysInfo = SystemInfoBar::Fetch();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MenuBarWindow::OnLButtonUp(int x, int y)
{
    if      (PointInRect(x, y, m_prevRect)) SendMediaKey(VK_MEDIA_PREV_TRACK);
    else if (PointInRect(x, y, m_playRect)) SendMediaKey(VK_MEDIA_PLAY_PAUSE);
    else if (PointInRect(x, y, m_nextRect)) SendMediaKey(VK_MEDIA_NEXT_TRACK);
}

void MenuBarWindow::OnMouseMove(int x, int y)
{
    FlyoutType hit = FlyoutType::None;

    if (PointInRect(x, y, m_widgetRects.clock))        hit = FlyoutType::Clock;
    else if (PointInRect(x, y, m_widgetRects.battery))  hit = FlyoutType::Battery;
    else if (PointInRect(x, y, m_widgetRects.volume))   hit = FlyoutType::Volume;
    else if (PointInRect(x, y, m_widgetRects.wifi))     hit = FlyoutType::Wifi;

    if (hit == m_hoveredWidget) return;
    m_hoveredWidget = hit;

    if (hit == FlyoutType::None)
    {
        m_flyout.Hide();
        return;
    }

    auto toScreen = [this](const RECT& r) {
        RECT s = r;
        POINT tl = { s.left, s.top };
        POINT br = { s.right, s.bottom };
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);
        return RECT{ tl.x, tl.y, br.x, br.y };
    };

    RECT anchor = {};
    if      (hit == FlyoutType::Clock)   anchor = toScreen(m_widgetRects.clock);
    else if (hit == FlyoutType::Battery) anchor = toScreen(m_widgetRects.battery);
    else if (hit == FlyoutType::Volume)  anchor = toScreen(m_widgetRects.volume);
    else if (hit == FlyoutType::Wifi)    anchor = toScreen(m_widgetRects.wifi);

    m_flyout.Show(hit, anchor, m_hwnd, m_sysInfo);
}

void MenuBarWindow::OnMouseLeave()
{
    m_hoveredWidget = FlyoutType::None;
    m_flyout.Hide();
}
