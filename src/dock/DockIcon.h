// DockIcon.h
// Represents a single pinned app icon in the Dock.
// Stores app metadata, extracted icon, current animation scale.

#pragma once
#include <windows.h>
#include <string>

class DockIcon
{
public:
    DockIcon(const std::wstring& appPath, const std::wstring& appName, HICON icon, HBITMAP bitmap = nullptr);
    ~DockIcon();

    // High-resolution premultiplied BGRA bitmap (256x256) from the shell image
    // factory. When present, OnPaint uses AlphaBlend for true bilinear
    // downscaling rather than DrawIconEx. May be nullptr (fallback icons).
    HBITMAP GetBitmap() const { return m_bitmap; }

    // Returns the app executable path
    const std::wstring& GetPath() const { return m_appPath; }

    // Returns the display name
    const std::wstring& GetName() const { return m_appName; }

    // Returns the extracted app icon handle
    HICON GetIcon() const { return m_icon; }

    // Current display scale (1.0 = normal, up to MAGNIFY_MAX during hover)
    float GetScale() const { return m_scale; }
    void  SetScale(float scale) { m_scale = scale; }

    // Whether a running indicator dot should be shown
    bool IsRunning() const { return m_isRunning; }
    void SetRunning(bool running) { m_isRunning = running; }

    // Bounding rect in Dock coordinates (set by DockWindow during layout)
    RECT GetBounds() const { return m_bounds; }
    void SetBounds(RECT bounds) { m_bounds = bounds; }

private:
    std::wstring m_appPath;
    std::wstring m_appName;
    HICON        m_icon;
    HBITMAP      m_bitmap;
    float        m_scale     = 1.0f;
    bool         m_isRunning = false;
    RECT         m_bounds    = {};
};
