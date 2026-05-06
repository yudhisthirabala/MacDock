// DockDropValidator.cpp
// Pure-logic file validation and name extraction for drag-and-drop.

#include "DockDropValidator.h"
#include <algorithm>

bool DockDropValidator::IsValidAppFile(const std::wstring& path)
{
    // Accept UWP/Store apps via shell:AppsFolder\ prefix
    if (path.size() > 17 && path.substr(0, 17) == L"shell:AppsFolder\\")
        return true;

    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;

    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    return (ext == L".exe" || ext == L".lnk");
}

std::wstring DockDropValidator::ExtractAppName(const std::wstring& path)
{
    // UWP apps: "shell:AppsFolder\Microsoft.WindowsStore_8wekyb3d8bbwe!App"
    // Extract the package family name as a readable-ish display name
    const std::wstring shellPrefix = L"shell:AppsFolder\\";
    if (path.size() > shellPrefix.size() &&
        path.substr(0, shellPrefix.size()) == shellPrefix)
    {
        std::wstring appId = path.substr(shellPrefix.size());
        // Take the part before the underscore: "Microsoft.WindowsStore"
        size_t underscore = appId.find(L'_');
        if (underscore != std::wstring::npos)
            appId = appId.substr(0, underscore);
        // Take the last dot-separated token: "WindowsStore"
        size_t lastDot = appId.find_last_of(L'.');
        if (lastDot != std::wstring::npos)
            appId = appId.substr(lastDot + 1);
        return appId;
    }

    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring filename = (lastSlash != std::wstring::npos)
        ? path.substr(lastSlash + 1)
        : path;

    size_t dot = filename.find_last_of(L'.');
    if (dot != std::wstring::npos)
        filename = filename.substr(0, dot);

    return filename;
}
