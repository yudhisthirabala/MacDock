// WidgetPopup.h
// macOS-style hover flyout panel for menu bar widgets.
// Shows contextual info (Wi-Fi, Volume, Battery, Clock) on hover.

#pragma once
#include <windows.h>
#include <string>
#include "SystemInfoBar.h"
#include "../system/CompositionHelper.h"

enum class WidgetType { None, Wifi, Volume, Battery, Clock };

class WidgetPopup
{
public:
    explicit WidgetPopup(HINSTANCE hInstance);
    ~WidgetPopup();

    bool Create();

    // Show the popup below the given screen-space anchor rect
    void ShowFor(WidgetType type, RECT anchorScreen, const SystemInfoData& data);

    // Hide the popup
    void Hide();

    // Returns true if the popup is currently visible
    bool IsVisible() const { return m_visible; }

    // Returns the current widget type being shown
    WidgetType GetType() const { return m_type; }

    // Returns the HWND
    HWND GetHWND() const { return m_hwnd; }

    // Check if a screen-space point is inside the popup
    bool HitTest(POINT screenPt) const;

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void Render();
    void RenderWifi(Gdiplus::Graphics& g, float s);
    void RenderVolume(Gdiplus::Graphics& g, float s);
    void RenderBattery(Gdiplus::Graphics& g, float s);
    void RenderClock(Gdiplus::Graphics& g, float s);

    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);

    float m_dpiScale = 1.0f;

    HINSTANCE      m_hInstance;
    HWND           m_hwnd      = nullptr;
    bool           m_visible   = false;
    WidgetType     m_type      = WidgetType::None;
    SystemInfoData m_data      = {};

    Composition::DCompWindow m_dcomp;

    // Volume slider state
    bool  m_draggingSlider = false;
    float m_sliderValue    = 0.0f;

    static constexpr int POPUP_W = 220;
    static constexpr int POPUP_H_WIFI    = 90;
    static constexpr int POPUP_H_VOLUME  = 80;
    static constexpr int POPUP_H_BATTERY = 75;
    static constexpr int POPUP_H_CLOCK   = 55;
    static constexpr int POPUP_GAP       = 4;
    static constexpr int CORNER_RADIUS   = 10;
};
