// ConfigManager.h
// Reads and writes the pinned apps configuration to %APPDATA%\macOSWin\pinned_apps.json.
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
    // Returns %APPDATA%\macOSWin\pinned_apps.json (DEC-028)
    static std::wstring GetConfigPath();

    // Load pinned apps from JSON. Creates empty file if missing.
    static std::vector<PinnedApp> Load();

    // Save pinned apps to JSON. Uses write-then-rename for crash safety.
    static bool Save(const std::vector<PinnedApp>& apps);

private:
    // Returns the legacy next-to-exe config path for migration
    static std::wstring GetLegacyConfigPath();

    // Migrates legacy config to new location if needed (DEC-028)
    static void MigrateLegacyConfig();
};
