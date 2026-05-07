// FlyoutWindow.cpp
// macOS-style flyout popup — dark rounded panel with widget details.

#include "FlyoutWindow.h"
#include "../system/SystemInfo.h"
#include <gdiplus.h>
#include <windowsx.h>
#include <iphlpapi.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wlanapi.h>
#include <cstring>
#include <cstdint>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")

using namespace Gdiplus;

static const wchar_t* FLYOUT_CLASS = L"macOSWin_Flyout";

static constexpr int FLYOUT_W       = 260;
static constexpr int FLYOUT_PADDING = 16;
static constexpr int LINE_HEIGHT    = 24;
static constexpr int CORNER_RADIUS  = 12;
static constexpr int TOP_GAP        = 6;
static constexpr int TITLE_FONT_PX  = 14;
static constexpr int BODY_FONT_PX   = 12;

namespace {

std::wstring GetAudioDeviceName()
{
    std::wstring name = L"Default Output";
    IMMDeviceEnumerator* enumer = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_INPROC_SERVER, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumer))))
        return name;

    IMMDevice* device = nullptr;
    if (SUCCEEDED(enumer->GetDefaultAudioEndpoint(eRender, eConsole, &device)) && device)
    {
        IPropertyStore* props = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props)
        {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.pwszVal)
                name = pv.pwszVal;
            PropVariantClear(&pv);
            props->Release();
        }
        device->Release();
    }
    enumer->Release();
    return name;
}

std::wstring GetLocalIPAddress()
{
    ULONG bufLen = 0;
    GetAdaptersInfo(nullptr, &bufLen);
    if (bufLen == 0) return L"Unavailable";

    std::vector<BYTE> buf(bufLen);
    auto* info = reinterpret_cast<IP_ADAPTER_INFO*>(buf.data());
    if (GetAdaptersInfo(info, &bufLen) != NO_ERROR)
        return L"Unavailable";

    for (auto* a = info; a; a = a->Next)
    {
        std::string ip = a->IpAddressList.IpAddress.String;
        if (ip.empty() || ip == "0.0.0.0") continue;
        return std::wstring(ip.begin(), ip.end());
    }
    return L"Unavailable";
}

std::wstring GetWifiSecurityType()
{
    HANDLE handle = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &handle) != ERROR_SUCCESS)
        return L"Unknown";

    std::wstring result = L"Open";
    WLAN_INTERFACE_INFO_LIST* ifList = nullptr;
    if (WlanEnumInterfaces(handle, nullptr, &ifList) == ERROR_SUCCESS && ifList)
    {
        for (DWORD i = 0; i < ifList->dwNumberOfItems; ++i)
        {
            const auto& info = ifList->InterfaceInfo[i];
            if (info.isState != wlan_interface_state_connected) continue;

            DWORD dataSize = 0;
            PVOID data = nullptr;
            WLAN_OPCODE_VALUE_TYPE opcode = wlan_opcode_value_type_invalid;
            if (WlanQueryInterface(handle, &info.InterfaceGuid,
                                   wlan_intf_opcode_current_connection, nullptr,
                                   &dataSize, &data, &opcode) == ERROR_SUCCESS && data)
            {
                auto* conn = static_cast<WLAN_CONNECTION_ATTRIBUTES*>(data);
                switch (conn->wlanSecurityAttributes.dot11AuthAlgorithm)
                {
                case DOT11_AUTH_ALGO_80211_OPEN:       result = L"Open"; break;
                case DOT11_AUTH_ALGO_80211_SHARED_KEY:  result = L"WEP"; break;
                case DOT11_AUTH_ALGO_WPA:               result = L"WPA"; break;
                case DOT11_AUTH_ALGO_WPA_PSK:           result = L"WPA-Personal"; break;
                case DOT11_AUTH_ALGO_RSNA:              result = L"WPA2-Enterprise"; break;
                case DOT11_AUTH_ALGO_RSNA_PSK:          result = L"WPA2-Personal"; break;
                default:
                    if (conn->wlanSecurityAttributes.dot11AuthAlgorithm >= 7)
                        result = L"WPA3";
                    else
                        result = L"Secured";
                    break;
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

std::wstring SignalLabel(int quality)
{
    if (quality >= 80) return L"Excellent";
    if (quality >= 60) return L"Good";
    if (quality >= 40) return L"Fair";
    if (quality >= 20) return L"Weak";
    return L"Poor";
}

} // namespace

FlyoutWindow::FlyoutWindow(HINSTANCE hInstance)
    : m_hInstance(hInstance)
{
}

FlyoutWindow::~FlyoutWindow()
{
    Hide();
    ReleaseDib();
}

void FlyoutWindow::Show(FlyoutType type, const RECT& anchorRect, HWND /*parentHwnd*/,
                         const SystemInfoData& data)
{
    if (m_visible) Hide();

    m_type = type;
    m_dpiScale = GetDpiForSystem() / 96.0f;
    m_lines.clear();

    auto S = [this](int v) { return static_cast<int>(v * m_dpiScale + 0.5f); };

    switch (type)
    {
    case FlyoutType::Clock:
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t dateFull[128] = {};
        wchar_t timeFull[32] = {};
        wchar_t dayName[32] = {};
        GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, dateFull, 128);
        GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, L"HH:mm:ss", timeFull, 32);
        GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, L"dddd", dayName, 32);

        TIME_ZONE_INFORMATION tzi = {};
        GetTimeZoneInformation(&tzi);

        // Week number (ISO 8601 approximation)
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        ULARGE_INTEGER ul;
        ul.LowPart = ft.dwLowDateTime;
        ul.HighPart = ft.dwHighDateTime;
        int dayOfYear = 0;
        for (int m = 1; m < st.wMonth; ++m) {
            static const int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
            dayOfYear += daysInMonth[m];
            if (m == 2 && (st.wYear % 4 == 0 && (st.wYear % 100 != 0 || st.wYear % 400 == 0)))
                dayOfYear += 1;
        }
        dayOfYear += st.wDay;
        int weekNum = (dayOfYear + 6) / 7;

        m_lines.push_back({ L"Date & Time", L"" });
        m_lines.push_back({ timeFull, L"" });
        m_lines.push_back({ dateFull, L"" });
        m_lines.push_back({ L"Timezone", tzi.StandardName });
        m_lines.push_back({ L"Week", L"Week " + std::to_wstring(weekNum) });
        m_lines.push_back({ L"Day of Year", std::to_wstring(dayOfYear) + L" / "
                            + std::to_wstring((st.wYear % 4 == 0 && (st.wYear % 100 != 0 || st.wYear % 400 == 0)) ? 366 : 365) });
        break;
    }
    case FlyoutType::Battery:
    {
        m_lines.push_back({ L"Battery", L"" });
        if (data.battery >= 0)
        {
            m_lines.push_back({ L"Level", std::to_wstring(data.battery) + L"%" });

            std::wstring status;
            if (data.charging && data.battery >= 100) status = L"Fully Charged";
            else if (data.charging)                   status = L"Charging";
            else                                      status = L"Discharging";
            m_lines.push_back({ L"Status", status });

            m_lines.push_back({ L"Power Source", data.charging ? L"Power Adapter" : L"Battery" });

            std::wstring condition;
            if (data.battery >= 80)      condition = L"Normal";
            else if (data.battery >= 40) condition = L"Normal";
            else if (data.battery >= 20) condition = L"Low";
            else                         condition = L"Critical";
            m_lines.push_back({ L"Condition", condition });

            if (!data.charging && data.batteryLifeTime > 0)
            {
                int hrs = data.batteryLifeTime / 3600;
                int mins = (data.batteryLifeTime % 3600) / 60;
                std::wstring remaining;
                if (hrs > 0) remaining = std::to_wstring(hrs) + L"h " + std::to_wstring(mins) + L"m";
                else         remaining = std::to_wstring(mins) + L" min";
                m_lines.push_back({ L"Time Remaining", remaining });
            }
            else if (!data.charging)
            {
                m_lines.push_back({ L"Time Remaining", L"Calculating…" });
            }
        }
        else
        {
            m_lines.push_back({ L"No battery detected", L"" });
            m_lines.push_back({ L"Power Source", L"AC Power" });
        }
        break;
    }
    case FlyoutType::Volume:
    {
        std::wstring deviceName = GetAudioDeviceName();

        m_lines.push_back({ L"Sound", L"" });
        m_lines.push_back({ L"Output Device", deviceName });
        m_lines.push_back({ L"Volume", std::to_wstring(data.volume) + L"%" });
        m_lines.push_back({ L"Status", data.muted ? L"Muted" : L"Active" });
        break;
    }
    case FlyoutType::Wifi:
    {
        m_lines.push_back({ L"Wi-Fi", L"" });
        if (data.wifiConnected)
        {
            m_lines.push_back({ L"Network", data.ssid.empty() ? L"Connected" : data.ssid });
            m_lines.push_back({ L"Signal", SignalLabel(data.wifiQuality)
                                + L" (" + std::to_wstring(data.wifiQuality) + L"%)" });
            m_lines.push_back({ L"Security", GetWifiSecurityType() });
            m_lines.push_back({ L"IP Address", GetLocalIPAddress() });
            m_lines.push_back({ L"Status", L"Connected" });
        }
        else
        {
            m_lines.push_back({ L"Status", L"Not Connected" });
            m_lines.push_back({ L"IP Address", L"None" });
        }
        break;
    }
    default: return;
    }

    int flyW = S(FLYOUT_W);
    int flyH = S(FLYOUT_PADDING * 2 + static_cast<int>(m_lines.size()) * LINE_HEIGHT);

    // Register class once
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = FlyoutWindow::WndProc;
    wc.hInstance      = m_hInstance;
    wc.lpszClassName  = FLYOUT_CLASS;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Position: centered below anchor, with a small gap
    int anchorCenterX = (anchorRect.left + anchorRect.right) / 2;
    int flyX = anchorCenterX - flyW / 2;
    int flyY = anchorRect.bottom + S(TOP_GAP);

    // Clamp to screen
    SIZE mon = { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (flyX + flyW > mon.cx) flyX = mon.cx - flyW;
    if (flyX < 0) flyX = 0;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        FLYOUT_CLASS, L"",
        WS_POPUP,
        flyX, flyY, flyW, flyH,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwnd) return;

    m_w = flyW;
    m_h = flyH;
    EnsureDib(flyW, flyH);
    Render();

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    m_visible = true;
}

void FlyoutWindow::Hide()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_visible = false;
    m_type = FlyoutType::None;
}

void FlyoutWindow::Render()
{
    if (!m_memDC || !m_bits) return;

    auto S = [this](int v) { return static_cast<int>(v * m_dpiScale + 0.5f); };
    auto SF = [this](float v) { return v * m_dpiScale; };

    std::memset(m_bits, 0, static_cast<size_t>(m_w) * m_h * 4);

    {
        Graphics g(m_memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetCompositingMode(CompositingModeSourceOver);

        // Dark rounded background
        float r = SF(static_cast<float>(CORNER_RADIUS));
        GraphicsPath bg;
        float fw = static_cast<float>(m_w);
        float fh = static_cast<float>(m_h);
        bg.AddArc(0.0f,          0.0f,          r * 2, r * 2, 180.0f, 90.0f);
        bg.AddArc(fw - r * 2,    0.0f,          r * 2, r * 2, 270.0f, 90.0f);
        bg.AddArc(fw - r * 2,    fh - r * 2,    r * 2, r * 2,   0.0f, 90.0f);
        bg.AddArc(0.0f,          fh - r * 2,    r * 2, r * 2,  90.0f, 90.0f);
        bg.CloseFigure();

        SolidBrush bgBrush(Color(230, 30, 30, 34));
        g.FillPath(&bgBrush, &bg);

        // 1px border
        Pen border(Color(80, 255, 255, 255), 1.0f);
        g.DrawPath(&border, &bg);

        FontFamily family(L"Segoe UI");
        Font titleFont(&family, SF(static_cast<float>(TITLE_FONT_PX)), FontStyleBold, UnitPixel);
        Font bodyFont(&family, SF(static_cast<float>(BODY_FONT_PX)), FontStyleRegular, UnitPixel);
        SolidBrush white(Color(255, 245, 245, 247));
        SolidBrush dim(Color(180, 180, 180, 185));

        float yPos = static_cast<float>(S(FLYOUT_PADDING));
        float xPad = static_cast<float>(S(FLYOUT_PADDING));
        float lineH = SF(static_cast<float>(LINE_HEIGHT));

        for (size_t i = 0; i < m_lines.size(); ++i)
        {
            const auto& [label, value] = m_lines[i];

            if (i == 0)
            {
                // Title line
                g.DrawString(label.c_str(), -1, &titleFont,
                             PointF(xPad, yPos), &white);
            }
            else if (value.empty())
            {
                // Single value line (centered-ish)
                g.DrawString(label.c_str(), -1, &bodyFont,
                             PointF(xPad, yPos), &white);
            }
            else
            {
                // Label: Value pair
                g.DrawString(label.c_str(), -1, &bodyFont,
                             PointF(xPad, yPos), &dim);

                RectF valBounds;
                g.MeasureString(value.c_str(), -1, &bodyFont, PointF(0, 0), &valBounds);
                float valX = fw - xPad - valBounds.Width;
                g.DrawString(value.c_str(), -1, &bodyFont,
                             PointF(valX, yPos), &white);
            }
            yPos += lineH;
        }
    }

    // Premultiply alpha
    auto* px = static_cast<uint8_t*>(m_bits);
    int count = m_w * m_h;
    for (int i = 0; i < count; ++i)
    {
        uint8_t a = px[3];
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
    SIZE  sz{ m_w, m_h };
    POINT src{ 0, 0 };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(m_hwnd, screen, &dst, &sz, m_memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK FlyoutWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FlyoutWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<FlyoutWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<FlyoutWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    (void)msg; (void)wParam; (void)lParam;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void FlyoutWindow::EnsureDib(int w, int h)
{
    ReleaseDib();
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screen);
    m_dib   = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);

    if (m_memDC && m_dib)
        m_oldBmp = static_cast<HBITMAP>(SelectObject(m_memDC, m_dib));
}

void FlyoutWindow::ReleaseDib()
{
    if (m_memDC && m_oldBmp) { SelectObject(m_memDC, m_oldBmp); m_oldBmp = nullptr; }
    if (m_dib)   { DeleteObject(m_dib); m_dib = nullptr; }
    if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
    m_bits = nullptr;
}
