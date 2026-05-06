// config_manager_test.cpp
// Unit tests for ConfigManager — round-trip, missing-file, malformed,
// and Unicode-path handling.
//
// Note: ConfigManager::GetConfigPath() resolves to the directory of the
// running executable. In tests that means pinned_apps.json lives next to
// macOSWin_tests.exe. Each test deletes the file on SetUp and TearDown
// so test order is irrelevant.

#include <gtest/gtest.h>
#include <windows.h>
#include <fstream>
#include "../src/config/ConfigManager.h"

namespace {

class ConfigManagerTest : public ::testing::Test
{
protected:
    void SetUp() override    { RemoveConfigFile(); }
    void TearDown() override { RemoveConfigFile(); }

    static void RemoveConfigFile()
    {
        const std::wstring p = ConfigManager::GetConfigPath();
        DeleteFileW(p.c_str());
    }

    static void WriteRaw(const std::string& contents)
    {
        std::ofstream f(ConfigManager::GetConfigPath());
        f << contents;
    }
};

TEST_F(ConfigManagerTest, LoadMissingFileReturnsEmpty)
{
    const auto apps = ConfigManager::Load();
    EXPECT_TRUE(apps.empty());
}

TEST_F(ConfigManagerTest, LoadMissingFileCreatesEmptyConfig)
{
    ConfigManager::Load();
    // Load() should have created an empty config on disk
    std::ifstream f(ConfigManager::GetConfigPath());
    ASSERT_TRUE(f.is_open());
}

TEST_F(ConfigManagerTest, SaveLoadRoundTrip)
{
    std::vector<PinnedApp> original = {
        { L"Notepad",    L"C:\\Windows\\notepad.exe" },
        { L"Calculator", L"C:\\Windows\\System32\\calc.exe" },
    };
    ASSERT_TRUE(ConfigManager::Save(original));

    const auto loaded = ConfigManager::Load();
    ASSERT_EQ(loaded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i)
    {
        EXPECT_EQ(loaded[i].name, original[i].name);
        EXPECT_EQ(loaded[i].path, original[i].path);
    }
}

TEST_F(ConfigManagerTest, LoadMalformedJsonReturnsEmpty)
{
    WriteRaw("this is not valid json {{{");
    const auto apps = ConfigManager::Load();
    EXPECT_TRUE(apps.empty());
}

TEST_F(ConfigManagerTest, LoadEntryWithEmptyPathIsSkipped)
{
    WriteRaw(R"([
        {"name":"HasPath","path":"C:\\a.exe"},
        {"name":"NoPath","path":""}
    ])");
    const auto apps = ConfigManager::Load();
    ASSERT_EQ(apps.size(), 1u);
    EXPECT_EQ(apps[0].name, L"HasPath");
}

TEST_F(ConfigManagerTest, UnicodeRoundTrip)
{
    // Exercise UTF-8 conversion path: accented Latin + CJK + emoji-plane char.
    std::vector<PinnedApp> original = {
        { L"café",    L"C:\\Users\\user\\Déjà vu.exe" },
        { L"日本語",   L"C:\\apps\\テスト.exe" },
    };
    ASSERT_TRUE(ConfigManager::Save(original));

    const auto loaded = ConfigManager::Load();
    ASSERT_EQ(loaded.size(), original.size());
    EXPECT_EQ(loaded[0].name, original[0].name);
    EXPECT_EQ(loaded[0].path, original[0].path);
    EXPECT_EQ(loaded[1].name, original[1].name);
    EXPECT_EQ(loaded[1].path, original[1].path);
}

TEST_F(ConfigManagerTest, SaveEmptyListProducesEmptyArray)
{
    ASSERT_TRUE(ConfigManager::Save({}));
    const auto apps = ConfigManager::Load();
    EXPECT_TRUE(apps.empty());
}

} // namespace
