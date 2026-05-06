// DockIcon.cpp
// Implementation of DockIcon — stores state for one pinned app.

#include "DockIcon.h"

DockIcon::DockIcon(const std::wstring& appPath, const std::wstring& appName, HICON icon, HBITMAP bitmap)
    : m_appPath(appPath), m_appName(appName), m_icon(icon), m_bitmap(bitmap)
{
}

DockIcon::~DockIcon()
{
    if (m_icon)   DestroyIcon(m_icon);
    if (m_bitmap) DeleteObject(m_bitmap);
}
