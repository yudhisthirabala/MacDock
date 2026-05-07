// SystemInfo.h
// Provides battery, volume, and Wi-Fi information via Win32 APIs.
// Phase 5 feature.

#pragma once
#include <windows.h>
#include <string>

struct BatteryInfo {
    int  percent;   // 0–100, or -1 if no battery
    bool charging;  // true if AC power connected
    int  lifeTime;  // seconds remaining on battery, -1 if unknown
};

struct VolumeInfo {
    int  percent;   // 0–100
    bool muted;
};

struct WifiInfo {
    bool         connected;
    std::wstring ssid;
    int          signalQuality; // 0–100
};

class SystemInfo
{
public:
    // Returns current battery status
    static BatteryInfo GetBattery();

    // Returns current system volume (default audio endpoint)
    static VolumeInfo GetVolume();

    // Returns current Wi-Fi connection status
    static WifiInfo GetWifi();
};
