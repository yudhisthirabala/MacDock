// drop_handler_test.cpp
// Unit tests for DockDropValidator — file-type validation (DEC-011 sub-1)
// and display-name extraction.
// These are pure-logic tests; actual WM_DROPFILES behavior is a manual test.

#include <gtest/gtest.h>
#include "dock/DockDropValidator.h"

namespace {

// ── IsValidAppFile ──────────────────────────────────────────────────────────

TEST(DropValidator, AcceptsExeFile)
{
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"C:\\Apps\\notepad.exe"));
}

TEST(DropValidator, AcceptsLnkFile)
{
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"C:\\Users\\me\\Desktop\\Chrome.lnk"));
}

TEST(DropValidator, AcceptsUppercaseExtension)
{
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"C:\\Apps\\Game.EXE"));
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"D:\\Shortcuts\\App.LNK"));
}

TEST(DropValidator, AcceptsMixedCaseExtension)
{
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"C:\\Tools\\util.Exe"));
    EXPECT_TRUE(DockDropValidator::IsValidAppFile(L"C:\\Links\\shortcut.Lnk"));
}

TEST(DropValidator, RejectsTxtFile)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\docs\\readme.txt"));
}

TEST(DropValidator, RejectsBatFile)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\scripts\\run.bat"));
}

TEST(DropValidator, RejectsCmdFile)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\scripts\\setup.cmd"));
}

TEST(DropValidator, RejectsUrlFile)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\links\\google.url"));
}

TEST(DropValidator, RejectsNoExtension)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\bin\\myapp"));
}

TEST(DropValidator, RejectsEmptyPath)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L""));
}

TEST(DropValidator, RejectsDirectoryLookingPath)
{
    EXPECT_FALSE(DockDropValidator::IsValidAppFile(L"C:\\Program Files\\"));
}

// ── ExtractAppName ──────────────────────────────────────────────────────────

TEST(DropValidator, ExtractsNameFromExePath)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"C:\\Windows\\notepad.exe"), L"notepad");
}

TEST(DropValidator, ExtractsNameFromLnkPath)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"C:\\Users\\me\\Desktop\\Chrome.lnk"), L"Chrome");
}

TEST(DropValidator, ExtractsNameWithSpaces)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"C:\\Program Files\\My App.exe"), L"My App");
}

TEST(DropValidator, ExtractsNameFromBareName)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"notepad.exe"), L"notepad");
}

TEST(DropValidator, ExtractsNameNoExtension)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"C:\\bin\\myapp"), L"myapp");
}

TEST(DropValidator, ExtractsNameUnicode)
{
    EXPECT_EQ(DockDropValidator::ExtractAppName(L"C:\\Apps\\テスト.exe"), L"テスト");
}

} // namespace
