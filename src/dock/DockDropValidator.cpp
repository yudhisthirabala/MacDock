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
    const std::wstring shellPrefix = L"shell:AppsFolder\\";
    if (path.size() > shellPrefix.size() &&
        path.substr(0, shellPrefix.size()) == shellPrefix)
    {
        std::wstring appId = path.substr(shellPrefix.size());

        // Known friendly names for common apps
        std::wstring lower = appId;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

        struct { const wchar_t* pattern; const wchar_t* name; } known[] = {
            { L"winword",               L"Word" },
            { L"excel",                 L"Excel" },
            { L"powerpnt",              L"PowerPoint" },
            { L"onenote",              L"OneNote" },
            { L"outlook",              L"Outlook" },
            { L"msedge",               L"Edge" },
            { L"chrome",               L"Chrome" },
            { L"immersivecontrolpanel", L"Settings" },
            { L"windowsterminal",       L"Terminal" },
            { L"windowscalculator",     L"Calculator" },
            { L"windowsstore",          L"Store" },
            { L"windowsnotepad",        L"Notepad" },
            { L"windowsalarms",         L"Clock" },
            { L"windowscamera",         L"Camera" },
            { L"windows.photos",        L"Photos" },
            { L"zunemusic",             L"Music" },
            { L"zunevideo",             L"Movies" },
            { L"whatsapp",              L"WhatsApp" },
            { L"gamingapp",             L"Xbox" },
            { L"paint",                 L"Paint" },
            { L"spotify",               L"Spotify" },
            { L"discord",               L"Discord" },
            { L"teams",                 L"Teams" },
            { L"clipchamp",             L"Clipchamp" },
            { L"snippingtool",          L"Snipping Tool" },
            { L"screenclipping",        L"Snipping Tool" },
        };
        for (const auto& k : known)
            if (lower.find(k.pattern) != std::wstring::npos)
                return k.name;

        // Fallback: take part before underscore, then last dot-token
        size_t underscore = appId.find(L'_');
        if (underscore != std::wstring::npos)
            appId = appId.substr(0, underscore);
        size_t lastDot = appId.find_last_of(L'.');
        if (lastDot != std::wstring::npos)
            appId = appId.substr(lastDot + 1);
        // Skip if result is purely numeric (version numbers like "15")
        bool allDigits = !appId.empty();
        for (wchar_t c : appId) if (!iswdigit(c)) { allDigits = false; break; }
        if (allDigits && lastDot != std::wstring::npos)
        {
            // Try second-to-last token
            std::wstring full = path.substr(shellPrefix.size());
            size_t us = full.find(L'_');
            if (us != std::wstring::npos) full = full.substr(0, us);
            size_t d2 = full.rfind(L'.', lastDot - 1);
            if (d2 != std::wstring::npos)
                appId = full.substr(d2 + 1, lastDot - d2 - 1);
        }
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
