// FlyoutWindow.h
// macOS-style dark rounded popup panel shown below menu bar widgets on click.

#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "SystemInfoBar.h"

enum class FlyoutType { None, Clock, Battery, Volume, Wifi };

class FlyoutWindow
{
public:
    explicit FlyoutWindow(HINSTANCE hInstance);
    ~FlyoutWindow();

    // Show a flyout anchored below the given screen rect
    void Show(FlyoutType type, const RECT& anchorRect, HWND parentHwnd,
              const SystemInfoData& data);

    // Hide and destroy the flyout
    void Hide();

    bool IsVisible() const { return m_visible; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();

    HINSTANCE  m_hInstance;
    HWND       m_hwnd     = nullptr;
    bool       m_visible  = false;
    FlyoutType m_type     = FlyoutType::None;
    float      m_dpiScale = 1.0f;

    // Content lines to render
    std::vector<std::pair<std::wstring, std::wstring>> m_lines;

    // Layered window DIB
    HDC     m_memDC   = nullptr;
    HBITMAP m_dib     = nullptr;
    HBITMAP m_oldBmp  = nullptr;
    void*   m_bits    = nullptr;
    int     m_w = 0, m_h = 0;

    void EnsureDib(int w, int h);
    void ReleaseDib();
};
