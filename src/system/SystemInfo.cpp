// SystemInfo.cpp
// Battery, volume, and Wi-Fi info via Win32 APIs. Phase 5.

#include "SystemInfo.h"

#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wlanapi.h>
#include <wtypes.h>

#pragma comment(lib, "wlanapi.lib")

BatteryInfo SystemInfo::GetBattery()
{
    SYSTEM_POWER_STATUS sps = {};
    if (!GetSystemPowerStatus(&sps)) return { -1, false };

    BatteryInfo info;
    info.percent  = (sps.BatteryLifePercent == 255) ? -1 : static_cast<int>(sps.BatteryLifePercent);
    info.charging = (sps.ACLineStatus == 1);
    // BatteryFlag bit 7 = "no system battery"
    if (sps.BatteryFlag & 128) info.percent = -1;
    return info;
}

VolumeInfo SystemInfo::GetVolume()
{
    VolumeInfo result = { 0, false };

    IMMDeviceEnumerator* enumer = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumer))))
        return result;

    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumer->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device)
    {
        IAudioEndpointVolume* vol = nullptr;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
                                       nullptr, reinterpret_cast<void**>(&vol))) && vol)
        {
            float scalar = 0.0f;
            if (SUCCEEDED(vol->GetMasterVolumeLevelScalar(&scalar)))
                result.percent = static_cast<int>(scalar * 100.0f + 0.5f);

            BOOL muted = FALSE;
            if (SUCCEEDED(vol->GetMute(&muted)))
                result.muted = (muted == TRUE);

            vol->Release();
        }
        device->Release();
    }
    enumer->Release();
    return result;
}

void SystemInfo::SetVolume(float scalar)
{
    if (scalar < 0.0f) scalar = 0.0f;
    if (scalar > 1.0f) scalar = 1.0f;

    IMMDeviceEnumerator* enumer = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumer))))
        return;

    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumer->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device)
    {
        IAudioEndpointVolume* vol = nullptr;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
                                       nullptr, reinterpret_cast<void**>(&vol))) && vol)
        {
            vol->SetMasterVolumeLevelScalar(scalar, nullptr);
            vol->Release();
        }
        device->Release();
    }
    enumer->Release();
}

WifiInfo SystemInfo::GetWifi()
{
    WifiInfo result = { false, L"", 0 };

    HANDLE handle = nullptr;
    DWORD  version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &handle) != ERROR_SUCCESS)
        return result;

    WLAN_INTERFACE_INFO_LIST* ifList = nullptr;
    if (WlanEnumInterfaces(handle, nullptr, &ifList) == ERROR_SUCCESS && ifList)
    {
        for (DWORD i = 0; i < ifList->dwNumberOfItems; ++i)
        {
            const WLAN_INTERFACE_INFO& info = ifList->InterfaceInfo[i];
            if (info.isState != wlan_interface_state_connected) continue;

            DWORD   dataSize = 0;
            PVOID   data     = nullptr;
            WLAN_OPCODE_VALUE_TYPE opcode = wlan_opcode_value_type_invalid;

            if (WlanQueryInterface(handle, &info.InterfaceGuid,
                                   wlan_intf_opcode_current_connection, nullptr,
                                   &dataSize, &data, &opcode) == ERROR_SUCCESS && data)
            {
                auto* conn = static_cast<WLAN_CONNECTION_ATTRIBUTES*>(data);
                result.connected     = true;
                result.signalQuality = static_cast<int>(conn->wlanAssociationAttributes.wlanSignalQuality);

                const DOT11_SSID& ssid = conn->wlanAssociationAttributes.dot11Ssid;
                if (ssid.uSSIDLength > 0)
                {
                    int cch = MultiByteToWideChar(CP_UTF8, 0,
                                                  reinterpret_cast<const char*>(ssid.ucSSID),
                                                  static_cast<int>(ssid.uSSIDLength),
                                                  nullptr, 0);
                    if (cch > 0)
                    {
                        result.ssid.resize(cch);
                        MultiByteToWideChar(CP_UTF8, 0,
                                            reinterpret_cast<const char*>(ssid.ucSSID),
                                            static_cast<int>(ssid.uSSIDLength),
                                            result.ssid.data(), cch);
                    }
                }
                WlanFreeMemory(data);
                break;
            }
        }
        WlanFreeMemory(ifList);
    }

    WlanCloseHandle(handle, nullptr);
    return result;
}
