// SystemInfoBar.cpp
// Renders the right-side widget cluster of the menu bar: Wi-Fi, volume, battery,
// and clock (right-to-left). Widgets are static (DEC-019). Uses GDI+ for
// anti-aliased glyph rendering so the look matches macOS vector crispness.

#include "SystemInfoBar.h"
#include "../system/SystemInfo.h"

#include <gdiplus.h>
#include <windows.h>

using namespace Gdiplus;

namespace {

constexpr int RIGHT_PADDING   = 14; // space between last widget and screen edge
constexpr int WIDGET_SPACING  = 14; // gap between widgets
constexpr int WIDGET_HEIGHT   = 14; // nominal glyph height

// DPI scale applied to every logical-px constant in this file. Computed once
// per Render call from the system DPI so widgets stay sharp and correctly
// sized on high-DPI displays instead of rendering at tiny physical px.
static float g_scale = 1.0f;
static inline int   SI(int v)   { return static_cast<int>(v * g_scale + 0.5f); }
static inline float SF(float v) { return v * g_scale; }

const Color kFg(235, 235, 235);

// ── Glyph renderers ──────────────────────────────────────────────────────────
// Each takes a right-edge x and vertical center y; returns the new right edge
// (i.e. the leftmost x consumed, so the caller can position the next widget).

int DrawWifi(Graphics& g, int rightX, int cy, bool connected, int quality)
{
    const int w = SI(14);
    const int h = SI(10);
    int leftX = rightX - w;

    SolidBrush fg(kFg);
    SolidBrush dim(Color(80, 235, 235, 235));

    // Three concentric arcs + center dot, drawn from the bottom up.
    // Each ring maps to a signal-quality threshold.
    int bars = 0;
    if (connected) {
        if      (quality >= 70) bars = 3;
        else if (quality >= 40) bars = 2;
        else if (quality >= 10) bars = 1;
    }

    g.SetSmoothingMode(SmoothingModeAntiAlias);

    float cx = static_cast<float>(rightX - w / 2);
    float by = static_cast<float>(cy + h / 2);  // bottom anchor

    // center dot
    {
        Brush* b = (bars >= 1) ? static_cast<Brush*>(&fg) : static_cast<Brush*>(&dim);
        g.FillEllipse(b, cx - SF(1.5f), by - SF(2.0f), SF(3.0f), SF(3.0f));
    }
    // three arcs (small, medium, large)
    for (int i = 0; i < 3; ++i)
    {
        float r    = SF(3.5f + i * 2.5f);
        Pen pen((bars >= i + 1) ? kFg : Color(80, 235, 235, 235), SF(1.6f));
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        g.DrawArc(&pen, cx - r, by - r - SF(1.0f), r * 2, r * 2, 210.0f, 120.0f);
    }
    return leftX;
}

int DrawVolume(Graphics& g, int rightX, int cy, int percent, bool muted)
{
    const int w = SI(16);
    const int h = SI(12);
    int leftX = rightX - w;

    g.SetSmoothingMode(SmoothingModeAntiAlias);

    SolidBrush fg(kFg);
    Pen        pen(kFg, SF(1.4f));
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);

    float x = static_cast<float>(leftX);
    float y = static_cast<float>(cy - h / 2);

    // Speaker body (trapezoid-ish: small rect + triangle cone)
    GraphicsPath body;
    body.AddRectangle(RectF(x, y + SF(3), SF(3), SF(6)));
    PointF cone[] = {
        PointF(x + SF(3), y + SF(2)),
        PointF(x + SF(8), y),
        PointF(x + SF(8), y + h),
        PointF(x + SF(3), y + h - SF(2))
    };
    body.AddPolygon(cone, 4);
    g.FillPath(&fg, &body);

    // Sound waves (show 0–3 arcs depending on volume; none if muted)
    int waves = 0;
    if (!muted) {
        if      (percent >= 66) waves = 3;
        else if (percent >= 33) waves = 2;
        else if (percent > 0)   waves = 1;
    }
    for (int i = 0; i < waves; ++i)
    {
        float r = SF(2.0f + i * 2.5f);
        g.DrawArc(&pen, x + SF(8) - r, y + h / 2 - r, r * 2, r * 2, -45.0f, 90.0f);
    }
    if (muted) {
        g.DrawLine(&pen, x + SF(10), y + SF(3),  x + SF(14), y + SF(9));
        g.DrawLine(&pen, x + SF(14), y + SF(3),  x + SF(10), y + SF(9));
    }
    return leftX;
}

int DrawBattery(Graphics& g, int rightX, int cy, int percent, bool charging)
{
    const int w   = SI(26);
    const int h   = SI(11);
    const int tip = SI(2);
    int leftX = rightX - (w + tip);

    g.SetSmoothingMode(SmoothingModeAntiAlias);

    Pen       pen(kFg, SF(1.2f));
    SolidBrush fg(kFg);

    float x = static_cast<float>(leftX);
    float y = static_cast<float>(cy - h / 2);
    float r = SF(3.0f);

    // Outline (rounded)
    GraphicsPath shell;
    shell.AddArc(x,              y,             r, r,  180.0f, 90.0f);
    shell.AddArc(x + w - r,      y,             r, r,  270.0f, 90.0f);
    shell.AddArc(x + w - r,      y + h - r,     r, r,    0.0f, 90.0f);
    shell.AddArc(x,              y + h - r,     r, r,   90.0f, 90.0f);
    shell.CloseFigure();
    g.DrawPath(&pen, &shell);

    // Tip nub
    g.FillRectangle(&fg, x + w, y + SF(3), static_cast<REAL>(tip), SF(5.0f));

    // Fill
    int   pct    = (percent < 0) ? 0 : (percent > 100 ? 100 : percent);
    float inset  = SF(2.0f);
    float fillW  = (w - inset * 2) * (pct / 100.0f);
    Color fillCl = kFg;
    if (!charging && pct <= 15) fillCl = Color(255, 230, 80, 80);
    SolidBrush fillBrush(fillCl);
    if (pct > 0)
        g.FillRectangle(&fillBrush, x + inset, y + inset, fillW, static_cast<REAL>(h) - inset * 2);

    // Charging bolt overlay
    if (charging) {
        SolidBrush bolt(Color(255, 80, 230, 120));
        PointF boltPts[] = {
            PointF(x + w / 2 + SF(1), y + SF(1)),
            PointF(x + w / 2 - SF(2), y + h / 2 + SF(0.5f)),
            PointF(x + w / 2,         y + h / 2 + SF(0.5f)),
            PointF(x + w / 2 - SF(2), y + h - SF(1)),
            PointF(x + w / 2 + SF(2), y + h / 2 - SF(0.5f)),
            PointF(x + w / 2,         y + h / 2 - SF(0.5f)),
        };
        g.FillPolygon(&bolt, boltPts, 6);
    }
    return leftX;
}

int DrawClock(Graphics& g, int rightX, int cy, const std::wstring& text)
{
    FontFamily family(L"Segoe UI");
    Font       font(&family, SF(12.0f), FontStyleRegular, UnitPixel);
    SolidBrush fg(kFg);

    RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, PointF(0, 0), &bounds);

    float x = static_cast<float>(rightX) - bounds.Width;
    float y = static_cast<float>(cy) - bounds.Height / 2.0f;

    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.DrawString(text.c_str(), -1, &font, PointF(x, y), &fg);

    return static_cast<int>(x);
}

} // namespace

SystemInfoData SystemInfoBar::Fetch()
{
    SystemInfoData d;

    BatteryInfo b = SystemInfo::GetBattery();
    d.battery         = b.percent;
    d.charging        = b.charging;
    d.batteryLifeTime = b.lifeTime;

    VolumeInfo v = SystemInfo::GetVolume();
    d.volume = v.muted ? 0 : v.percent;
    d.muted  = v.muted;

    WifiInfo w = SystemInfo::GetWifi();
    d.wifiConnected = w.connected;
    d.wifiQuality   = w.signalQuality;
    d.ssid          = w.ssid;

    // macOS-style date: "Thu 17 Apr  14:32"
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t datePart[64] = {};
    wchar_t timePart[16] = {};
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"ddd d MMM", datePart, 64);
    GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS | TIME_FORCE24HOURFORMAT,
                   &st, L"HH:mm", timePart, 16);

    d.clock = std::wstring(datePart) + L"  " + timePart;
    return d;
}

int SystemInfoBar::Render(HDC hdc, RECT barRect, const SystemInfoData& data,
                          WidgetHitRects* outRects)
{
    g_scale = GetDpiForSystem() / 96.0f;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    int cy      = (barRect.top + barRect.bottom) / 2;
    int spacing = SI(WIDGET_SPACING);
    int right   = barRect.right - SI(RIGHT_PADDING);
    int prevRight;

    // Right-to-left: clock, battery, volume, wifi
    prevRight = right;
    right = DrawClock(g, right, cy, data.clock);
    if (outRects) outRects->clock = { right, barRect.top, prevRight, barRect.bottom };
    right -= spacing;

    if (data.battery >= 0) {
        prevRight = right;
        right = DrawBattery(g, right, cy, data.battery, data.charging);
        if (outRects) outRects->battery = { right, barRect.top, prevRight, barRect.bottom };
        right -= spacing;
    }

    prevRight = right;
    right = DrawVolume(g, right, cy, data.volume, data.volume == 0);
    if (outRects) outRects->volume = { right, barRect.top, prevRight, barRect.bottom };
    right -= spacing;

    prevRight = right;
    right = DrawWifi(g, right, cy, data.wifiConnected, data.wifiQuality);
    if (outRects) outRects->wifi = { right, barRect.top, prevRight, barRect.bottom };
    right -= spacing;

    return right;
}
