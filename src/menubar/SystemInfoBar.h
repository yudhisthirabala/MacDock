// SystemInfoBar.h
// Renders clock, battery, volume, and Wi-Fi status on the right of the menu bar.

#pragma once
#include <windows.h>
#include <string>

struct SystemInfoData
{
    std::wstring clock;           // e.g. "14:32"
    int          battery;         // 0–100, or -1 if no battery
    bool         charging;        // true if AC plugged in
    int          batteryLifeTime; // seconds remaining on battery, -1 if unknown
    int          volume;          // 0–100
    bool         muted;           // true if system audio is muted
    bool         wifiConnected;
    int          wifiQuality;     // 0–100 signal quality (0 if not connected)
    std::wstring ssid;            // connected network name
};

// Hit-test rects for clickable widgets
struct WidgetHitRects
{
    RECT clock   = {};
    RECT battery = {};
    RECT volume  = {};
    RECT wifi    = {};
};

class SystemInfoBar
{
public:
    // Fetch all system info into a SystemInfoData struct
    static SystemInfoData Fetch();

    // Render the system info section into a DC at a given x offset from the right edge
    // Returns the leftmost x used (so the caller can avoid overlap)
    static int Render(HDC hdc, RECT barRect, const SystemInfoData& data,
                      WidgetHitRects* outRects = nullptr);
};
