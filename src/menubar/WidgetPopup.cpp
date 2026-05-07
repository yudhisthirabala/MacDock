// WidgetPopup.cpp
// macOS-style hover flyout panel. Renders contextual info for each widget type.
// Uses the same layered-window + GDI+ pipeline as the dock and menu bar.

#include "WidgetPopup.h"
#include "../system/SystemInfo.h"
#include <gdiplus.h>
#include <cmath>

using namespace Gdiplus;

static const wchar_t* POPUP_CLASS_NAME = L"macOSWin_WidgetPopup";

// Dark translucent background matching the dock pill
static const Color kPopupBg(210, 30, 30, 34);
static const Color kPopupBorder(50, 255, 255, 255);
static const Color kFg(255, 245, 245, 247);
static const Color kFgDim(180, 180, 180, 185);
static const Color kAccent(255, 0, 122, 255);
static const Color kSliderTrack(80, 255, 255, 255);
static const Color kSliderFill(255, 255, 255, 255);

// Slider layout constants (logical px)
static constexpr float SLIDER_LEFT   = 16.0f;
static constexpr float SLIDER_RIGHT  = 16.0f;
static constexpr float SLIDER_Y      = 50.0f;
static constexpr float SLIDER_H      = 4.0f;
static constexpr float KNOB_RADIUS   = 7.0f;

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

void FillRoundedRect(Graphics& g, const Color& c, float x, float y, float w, float h, float r)
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

void DrawRoundedRectOutline(Graphics& g, const Color& c, float x, float y, float w, float h, float r)
{
    GraphicsPath p;
    p.AddArc(x,             y,             r * 2, r * 2, 180.0f, 90.0f);
    p.AddArc(x + w - r * 2, y,             r * 2, r * 2, 270.0f, 90.0f);
    p.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2,   0.0f, 90.0f);
    p.AddArc(x,             y + h - r * 2, r * 2, r * 2,  90.0f, 90.0f);
    p.CloseFigure();
    Pen pen(c, 0.5f);
    g.DrawPath(&pen, &p);
}

} // namespace

// ─── Constructor / Destructor ────────────────────────────────────────────────

WidgetPopup::WidgetPopup(HINSTANCE hInstance)
    : m_hInstance(hInstance)
{
}

WidgetPopup::~WidgetPopup()
{
    m_dcomp.Release();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// ─── Create ──────────────────────────────────────────────────────────────────

bool WidgetPopup::Create()
{
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WidgetPopup::WndProc;
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = POPUP_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    m_dpiScale = GetDpiForSystem() / 96.0f;

    int pw = static_cast<int>(POPUP_W * m_dpiScale + 0.5f);
    int ph = static_cast<int>(POPUP_H_VOLUME * m_dpiScale + 0.5f);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        POPUP_CLASS_NAME, L"WidgetPopup",
        WS_POPUP,
        -9999, -9999, pw, ph,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) return false;
    if (!m_dcomp.Init(m_hwnd, pw, ph)) return false;

    return true;
}

// ─── ShowFor ─────────────────────────────────────────────────────────────────

void WidgetPopup::ShowFor(WidgetType type, RECT anchorScreen, const SystemInfoData& data)
{
    m_type = type;
    m_data = data;

    int popupH = 0;
    switch (type) {
        case WidgetType::Wifi:    popupH = POPUP_H_WIFI;    break;
        case WidgetType::Volume:  popupH = POPUP_H_VOLUME;  break;
        case WidgetType::Battery: popupH = POPUP_H_BATTERY; break;
        case WidgetType::Clock:   popupH = POPUP_H_CLOCK;   break;
        default: return;
    }

    int pw = static_cast<int>(POPUP_W * m_dpiScale + 0.5f);
    int ph = static_cast<int>(popupH * m_dpiScale + 0.5f);
    int gap = static_cast<int>(POPUP_GAP * m_dpiScale + 0.5f);

    int anchorCx = (anchorScreen.left + anchorScreen.right) / 2;
    int x = anchorCx - pw / 2;
    int y = anchorScreen.bottom + gap;

    SIZE mon = Composition::GetPrimaryMonitorSize();
    if (x + pw > mon.cx) x = mon.cx - pw - 4;
    if (x < 4) x = 4;

    m_dcomp.Resize(pw, ph);
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);

    if (type == WidgetType::Volume)
        m_sliderValue = data.volume / 100.0f;

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    m_visible = true;
    Render();
}

void WidgetPopup::Hide()
{
    if (!m_visible) return;
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
    m_type = WidgetType::None;
    m_draggingSlider = false;
}

bool WidgetPopup::HitTest(POINT screenPt) const
{
    if (!m_visible) return false;
    RECT wr;
    GetWindowRect(m_hwnd, &wr);
    return PtInRect(&wr, screenPt) != FALSE;
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

LRESULT CALLBACK WidgetPopup::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WidgetPopup* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<WidgetPopup*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<WidgetPopup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        self->Render();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        self->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        self->OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_LBUTTONUP:
        self->OnLButtonUp(LOWORD(lParam), HIWORD(lParam));
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ─── Render ──────────────────────────────────────────────────────────────────

void WidgetPopup::Render()
{
    POINT offset;
    HDC hdc = m_dcomp.BeginDraw(&offset);
    if (!hdc) return;

    const int w = m_dcomp.w;
    const int h = m_dcomp.h;
    const float s = m_dpiScale;

    {
        Graphics g(hdc);
        g.TranslateTransform(static_cast<REAL>(offset.x), static_cast<REAL>(offset.y));
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

        g.SetCompositingMode(CompositingModeSourceCopy);
        g.Clear(Color(0, 0, 0, 0));
        g.SetCompositingMode(CompositingModeSourceOver);

        float r = CORNER_RADIUS * s;
        FillRoundedRect(g, kPopupBg, 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), r);
        DrawRoundedRectOutline(g, kPopupBorder, 0.0f, 0.0f,
                               static_cast<float>(w), static_cast<float>(h), r);

        switch (m_type) {
            case WidgetType::Wifi:    RenderWifi(g, s);    break;
            case WidgetType::Volume:  RenderVolume(g, s);  break;
            case WidgetType::Battery: RenderBattery(g, s); break;
            case WidgetType::Clock:   RenderClock(g, s);   break;
            default: break;
        }
    }

    m_dcomp.EndDraw();
    m_dcomp.Commit();
}

void WidgetPopup::RenderWifi(Graphics& g, float s)
{
    FontFamily fam(L"Segoe UI");
    Font titleFont(&fam, 13.0f * s, FontStyleBold, UnitPixel);
    Font bodyFont(&fam, 12.0f * s, FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);
    SolidBrush dim(kFgDim);

    g.DrawString(L"Wi‑Fi", -1, &titleFont, PointF(16.0f * s, 12.0f * s), &fg);

    if (m_data.wifiConnected && !m_data.ssid.empty())
    {
        g.DrawString(m_data.ssid.c_str(), -1, &bodyFont, PointF(16.0f * s, 34.0f * s), &fg);

        std::wstring quality;
        if (m_data.wifiQuality >= 70)      quality = L"Signal: Strong";
        else if (m_data.wifiQuality >= 40) quality = L"Signal: Medium";
        else                               quality = L"Signal: Weak";
        g.DrawString(quality.c_str(), -1, &bodyFont, PointF(16.0f * s, 56.0f * s), &dim);
    }
    else
    {
        g.DrawString(L"Not Connected", -1, &bodyFont, PointF(16.0f * s, 34.0f * s), &dim);
    }
}

void WidgetPopup::RenderVolume(Graphics& g, float s)
{
    FontFamily fam(L"Segoe UI");
    Font titleFont(&fam, 13.0f * s, FontStyleBold, UnitPixel);
    Font bodyFont(&fam, 11.0f * s, FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);
    SolidBrush dim(kFgDim);

    g.DrawString(L"Sound", -1, &titleFont, PointF(16.0f * s, 12.0f * s), &fg);

    int pct = static_cast<int>(m_sliderValue * 100.0f + 0.5f);
    std::wstring volText = std::to_wstring(pct) + L"%";
    RectF textBox(0, 12.0f * s, static_cast<REAL>(m_dcomp.w) - 16.0f * s, 18.0f * s);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentFar);
    g.DrawString(volText.c_str(), -1, &bodyFont, textBox, &sf, &dim);

    // Slider track
    float sliderL = SLIDER_LEFT * s;
    float sliderR = static_cast<float>(m_dcomp.w) - SLIDER_RIGHT * s;
    float sliderW = sliderR - sliderL;
    float sliderY = SLIDER_Y * s;
    float trackH  = SLIDER_H * s;

    SolidBrush trackBrush(kSliderTrack);
    FillRoundedRect(g, kSliderTrack, sliderL, sliderY - trackH / 2.0f,
                    sliderW, trackH, trackH / 2.0f);

    // Filled portion
    float fillW = sliderW * m_sliderValue;
    if (fillW > 0.0f)
        FillRoundedRect(g, kSliderFill, sliderL, sliderY - trackH / 2.0f,
                        fillW, trackH, trackH / 2.0f);

    // Knob
    float knobX = sliderL + fillW;
    float knobR = KNOB_RADIUS * s;
    SolidBrush knobBrush(Color(255, 255, 255, 255));
    g.FillEllipse(&knobBrush, knobX - knobR, sliderY - knobR, knobR * 2.0f, knobR * 2.0f);

    // Subtle knob shadow
    Pen knobOutline(Color(40, 0, 0, 0), 0.5f * s);
    g.DrawEllipse(&knobOutline, knobX - knobR, sliderY - knobR, knobR * 2.0f, knobR * 2.0f);
}

void WidgetPopup::RenderBattery(Graphics& g, float s)
{
    FontFamily fam(L"Segoe UI");
    Font titleFont(&fam, 13.0f * s, FontStyleBold, UnitPixel);
    Font bodyFont(&fam, 12.0f * s, FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);
    SolidBrush dim(kFgDim);

    g.DrawString(L"Battery", -1, &titleFont, PointF(16.0f * s, 12.0f * s), &fg);

    if (m_data.battery >= 0)
    {
        std::wstring pctText = std::to_wstring(m_data.battery) + L"%";
        RectF textBox(0, 12.0f * s, static_cast<REAL>(m_dcomp.w) - 16.0f * s, 18.0f * s);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentFar);
        g.DrawString(pctText.c_str(), -1, &bodyFont, textBox, &sf, &fg);

        // Battery bar
        float barL = 16.0f * s;
        float barW = static_cast<float>(m_dcomp.w) - 32.0f * s;
        float barH = 6.0f * s;
        float barY = 42.0f * s;

        SolidBrush trackBrush(kSliderTrack);
        FillRoundedRect(g, kSliderTrack, barL, barY, barW, barH, barH / 2.0f);

        Color fillColor = kSliderFill;
        if (m_data.battery <= 15 && !m_data.charging)
            fillColor = Color(255, 230, 80, 80);
        else if (m_data.charging)
            fillColor = Color(255, 80, 230, 120);

        float fillW = barW * (m_data.battery / 100.0f);
        if (fillW > 0.0f)
            FillRoundedRect(g, fillColor, barL, barY, fillW, barH, barH / 2.0f);

        std::wstring status = m_data.charging ? L"Charging" : L"On Battery";
        g.DrawString(status.c_str(), -1, &bodyFont, PointF(16.0f * s, 54.0f * s), &dim);
    }
    else
    {
        g.DrawString(L"No Battery", -1, &bodyFont, PointF(16.0f * s, 34.0f * s), &dim);
    }
}

void WidgetPopup::RenderClock(Graphics& g, float s)
{
    FontFamily fam(L"Segoe UI");
    Font titleFont(&fam, 13.0f * s, FontStyleBold, UnitPixel);
    Font bodyFont(&fam, 12.0f * s, FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t fullDate[128] = {};
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, fullDate, 128);

    g.DrawString(fullDate, -1, &titleFont, PointF(16.0f * s, 14.0f * s), &fg);

    wchar_t timePart[32] = {};
    GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, L"HH:mm:ss", timePart, 32);
    g.DrawString(timePart, -1, &bodyFont, PointF(16.0f * s, 34.0f * s), &fg);
}

// ─── Mouse handlers (for volume slider) ─────────────────────────────────────

void WidgetPopup::OnMouseMove(int x, int y)
{
    if (m_draggingSlider && m_type == WidgetType::Volume)
    {
        float s = m_dpiScale;
        float sliderL = SLIDER_LEFT * s;
        float sliderR = static_cast<float>(m_dcomp.w) - SLIDER_RIGHT * s;
        float sliderW = sliderR - sliderL;

        float val = (static_cast<float>(x) - sliderL) / sliderW;
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        m_sliderValue = val;

        SystemInfo::SetVolume(val);
        Render();
    }
}

void WidgetPopup::OnLButtonDown(int x, int y)
{
    if (m_type == WidgetType::Volume)
    {
        float s = m_dpiScale;
        float sliderL = SLIDER_LEFT * s;
        float sliderR = static_cast<float>(m_dcomp.w) - SLIDER_RIGHT * s;
        float sliderW = sliderR - sliderL;
        float sliderY = SLIDER_Y * s;
        float knobR   = KNOB_RADIUS * s;

        // Hit test the slider area (generous vertical range)
        if (y >= sliderY - knobR * 2 && y <= sliderY + knobR * 2 &&
            x >= sliderL - knobR && x <= sliderR + knobR)
        {
            m_draggingSlider = true;
            SetCapture(m_hwnd);

            float val = (static_cast<float>(x) - sliderL) / sliderW;
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            m_sliderValue = val;

            SystemInfo::SetVolume(val);
            Render();
        }
    }
}

void WidgetPopup::OnLButtonUp(int /*x*/, int /*y*/)
{
    if (m_draggingSlider)
    {
        m_draggingSlider = false;
        ReleaseCapture();
    }
}
