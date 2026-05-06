// ConfigManager.cpp
// JSON config read/write using nlohmann/json.

#include "ConfigManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <windows.h>

using json = nlohmann::json;

// Helpers to convert between std::wstring and std::string (UTF-8)
static std::string WtoU8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, result.data(), len, nullptr, nullptr);
    return result;
}

static std::wstring U8toW(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), len);
    return result;
}

std::wstring ConfigManager::GetConfigPath()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        path = path.substr(0, lastSlash + 1);
    return path + L"pinned_apps.json";
}

std::vector<PinnedApp> ConfigManager::Load()
{
    std::vector<PinnedApp> apps;
    std::wstring configPath = GetConfigPath();

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        // Create empty config
        Save(apps);
        return apps;
    }

    try
    {
        json j;
        file >> j;
        for (const auto& item : j)
        {
            PinnedApp app;
            app.name = U8toW(item.value("name", ""));
            app.path = U8toW(item.value("path", ""));
            if (!app.path.empty())
                apps.push_back(app);
        }
    }
    catch (...) { /* malformed JSON — return empty */ }

    return apps;
}

bool ConfigManager::Save(const std::vector<PinnedApp>& apps)
{
    json j = json::array();
    for (const auto& app : apps)
    {
        j.push_back({
            { "name", WtoU8(app.name) },
            { "path", WtoU8(app.path) }
        });
    }

    std::wstring configPath = GetConfigPath();
    std::wstring tempPath   = configPath + L".tmp";

    // Write to temp file first (crash-safe)
    {
        std::ofstream tmp(tempPath);
        if (!tmp.is_open()) return false;
        tmp << j.dump(2);
    }

    // Atomic replace
    return MoveFileExW(tempPath.c_str(), configPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
}
