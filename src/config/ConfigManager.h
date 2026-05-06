// ConfigManager.h
// Reads and writes the pinned apps configuration to pinned_apps.json.
// Uses nlohmann/json (header-only).

#pragma once
#include <string>
#include <vector>

struct PinnedApp {
    std::wstring name;    // Display name shown in Dock tooltip
    std::wstring path;    // Full path to .exe or .lnk
};

class ConfigManager
{
public:
    // Returns the path to pinned_apps.json (next to the running exe)
    static std::wstring GetConfigPath();

    // Load pinned apps from JSON. Creates empty file if missing.
    static std::vector<PinnedApp> Load();

    // Save pinned apps to JSON. Uses write-then-rename for crash safety.
    static bool Save(const std::vector<PinnedApp>& apps);
};
