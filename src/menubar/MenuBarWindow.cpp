// MenuBarWindow.cpp
// Top menu bar: media-control cluster · active app name · Now-Playing text · system info.
// Phase 6 (DEC-027): DirectComposition rendering replaces WS_EX_LAYERED / BitBlt.
// DWMSBT_TRANSIENTWINDOW provides real OS acrylic on Win11 22H2+.

#include "MenuBarWindow.h"
#include "SystemInfoBar.h"
#include "../system/CompositionHelper.h"

#include <gdiplus.h>
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
    : m_hInstance(hInstance), m_hwnd(nullptr), m_activeAppName(L"Dock")
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
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
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

    m_popup = std::make_unique<WidgetPopup>(m_hInstance);
    if (!m_popup->Create()) m_popup.reset();

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
    int barH = static_cast<int>(BAR_HEIGHT * m_dpiScale + 0.5f);
    SIZE mon = Composition::GetPrimaryMonitorSize();

    // Start above the screen and slide down
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, -barH, mon.cx, barH, SWP_NOACTIVATE);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    m_entranceActive  = true;
    m_entranceStartMs = 0;  // set on first timer tick
    RenderDComp();
    SetTimer(m_hwnd, TIMER_ENTRANCE, 16, nullptr);
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
        if (wParam == TIMER_ENTRANCE)
        {
            if (self->m_entranceStartMs == 0)
                self->m_entranceStartMs = GetTickCount();
            int barH = static_cast<int>(BAR_HEIGHT * self->m_dpiScale + 0.5f);
            DWORD elapsed = GetTickCount() - self->m_entranceStartMs;
            if (elapsed >= self->ENTRANCE_DURATION)
            {
                self->m_entranceActive = false;
                KillTimer(hwnd, TIMER_ENTRANCE);
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
                self->RenderDComp();
                return 0;
            }
            float T = static_cast<float>(self->ENTRANCE_DURATION);
            float t = static_cast<float>(elapsed);
            float ease = (2.0f * t / T) - (t * t) / (T * T);
            int curY = -barH + static_cast<int>(barH * ease);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, curY, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
            self->RenderDComp();
            return 0;
        }
        if (wParam == TIMER_POPUP_HIDE)
        {
            KillTimer(hwnd, TIMER_POPUP_HIDE);
            if (self->m_popup && self->m_popup->IsVisible())
            {
                POINT cur; GetCursorPos(&cur);
                if (!self->m_popup->HitTest(cur))
                {
                    POINT client = cur;
                    ScreenToClient(hwnd, &client);
                    auto hitWidget = [&](const RECT& r) {
                        return client.x >= r.left && client.x < r.right &&
                               client.y >= r.top  && client.y < r.bottom;
                    };
                    bool overWidget = hitWidget(self->m_widgetRects.wifi) ||
                                      hitWidget(self->m_widgetRects.volume) ||
                                      hitWidget(self->m_widgetRects.battery) ||
                                      hitWidget(self->m_widgetRects.clock);
                    if (!overWidget)
                        self->m_popup->Hide();
                }
            }
            return 0;
        }
        self->OnTimer();
        return 0;

    case WM_MOUSEMOVE:
        self->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSELEAVE:
        self->OnMouseLeave();
        return 0;

    case WM_LBUTTONUP:
        self->OnLButtonUp(LOWORD(lParam), HIWORD(lParam));
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

        Pen hilite(Color(30, 255, 255, 255), 1.0f);
        g.DrawLine(&hilite, 0, 0, w, 0);
        Pen edge(Color(60, 0, 0, 0), 1.0f);
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

// ─── Hover tracking ──────────────────────────────────────────────────────────

void MenuBarWindow::OnMouseMove(int x, int y)
{
    if (!m_mouseTracking)
    {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_mouseTracking = true;
    }

    auto hitWidget = [x, y](const RECT& r) {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    };

    WidgetType hovered = WidgetType::None;
    RECT anchorRect = {};
    if      (hitWidget(m_widgetRects.wifi))    { hovered = WidgetType::Wifi;    anchorRect = m_widgetRects.wifi; }
    else if (hitWidget(m_widgetRects.volume))  { hovered = WidgetType::Volume;  anchorRect = m_widgetRects.volume; }
    else if (hitWidget(m_widgetRects.battery)) { hovered = WidgetType::Battery; anchorRect = m_widgetRects.battery; }
    else if (hitWidget(m_widgetRects.clock))   { hovered = WidgetType::Clock;   anchorRect = m_widgetRects.clock; }

    if (hovered != WidgetType::None && m_popup)
    {
        KillTimer(m_hwnd, TIMER_POPUP_HIDE);

        if (hovered != m_hoveredWidget || !m_popup->IsVisible())
        {
            // Convert anchor rect to screen coords
            RECT screenRect = anchorRect;
            POINT tl = { screenRect.left, screenRect.top };
            POINT br = { screenRect.right, screenRect.bottom };
            ClientToScreen(m_hwnd, &tl);
            ClientToScreen(m_hwnd, &br);
            screenRect = { tl.x, tl.y, br.x, br.y };

            m_popup->ShowFor(hovered, screenRect, m_sysInfo);
            m_hoveredWidget = hovered;
        }
    }
    else if (hovered == WidgetType::None && m_popup && m_popup->IsVisible())
    {
        SetTimer(m_hwnd, TIMER_POPUP_HIDE, POPUP_HIDE_DELAY, nullptr);
    }
}

void MenuBarWindow::OnMouseLeave()
{
    m_mouseTracking = false;
    if (m_popup && m_popup->IsVisible())
        SetTimer(m_hwnd, TIMER_POPUP_HIDE, POPUP_HIDE_DELAY, nullptr);
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
