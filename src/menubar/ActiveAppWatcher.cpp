// ActiveAppWatcher.cpp
// Foreground window monitoring via SetWinEventHook (DEC-020).
// Resolves the foreground HWND to a clean app name via exe version-info
// FileDescription, falling back to the exe basename.

#include "ActiveAppWatcher.h"
#include <psapi.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cwctype>

#pragma comment(lib, "version.lib")

HWINEVENTHOOK             ActiveAppWatcher::s_hook     = nullptr;
ActiveAppWatcher::Callback ActiveAppWatcher::s_callback = nullptr;

namespace {

// Cache exe path -> friendly name to avoid re-querying version info on every focus flip.
std::unordered_map<std::wstring, std::wstring>& NameCache()
{
    static std::unordered_map<std::wstring, std::wstring> c;
    return c;
}

// Desktop / shell surfaces — when focus lands here, the user is effectively
// "on the desktop" with no app focused, so the bar should reset to "Dock".
bool IsDesktopClass(const std::wstring& cls)
{
    static const std::unordered_set<std::wstring> desktop = {
        L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow", L"TrayNotifyWnd",
        L"CabinetWClass", L"ExplorerWClass",
        L"Windows.UI.Core.CoreWindow"  // Start menu / search
    };
    return desktop.count(cls) != 0;
}

// Our own overlay windows — skip entirely (keep whatever name was showing)
// since clicking the dock/menubar shouldn't itself change the bar text.
bool IsOwnOverlayClass(const std::wstring& cls)
{
    static const std::unordered_set<std::wstring> own = {
        L"macOSWin_MenuBar", L"macOSWin_Dock", L"macOSWin_TrayReceiver"
    };
    return own.count(cls) != 0;
}

// Filter by exe filename — catches any other explorer.exe-hosted popup
// (jump lists, context menus, taskbar flyouts) that wasn't in the class list.
bool IsShellExe(const std::wstring& exePath)
{
    size_t slash = exePath.find_last_of(L"\\/");
    std::wstring name = (slash != std::wstring::npos) ? exePath.substr(slash + 1) : exePath;
    std::transform(name.begin(), name.end(), name.begin(), ::towlower);
    return name == L"explorer.exe" || name == L"searchhost.exe" ||
           name == L"startmenuexperiencehost.exe" || name == L"shellexperiencehost.exe";
}

std::wstring ExeBasename(const std::wstring& fullPath)
{
    size_t slash = fullPath.find_last_of(L"\\/");
    std::wstring name = (slash != std::wstring::npos) ? fullPath.substr(slash + 1) : fullPath;
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    if (!name.empty()) name[0] = static_cast<wchar_t>(std::towupper(name[0]));
    return name;
}

// Pull FileDescription from the exe's version-info resource. macOS-authentic names
// ("Google Chrome" instead of "chrome", "Visual Studio Code" instead of "Code").
std::wstring FileDescription(const std::wstring& exePath)
{
    DWORD dummy = 0;
    DWORD size  = GetFileVersionInfoSizeW(exePath.c_str(), &dummy);
    if (!size) return L"";

    std::vector<BYTE> buf(size);
    if (!GetFileVersionInfoW(exePath.c_str(), 0, size, buf.data())) return L"";

    struct LangCp { WORD lang; WORD cp; };
    LangCp* translations = nullptr;
    UINT    tLen = 0;
    if (!VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translations), &tLen) || tLen < sizeof(LangCp))
        return L"";

    wchar_t subBlock[64];
    swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription",
               translations[0].lang, translations[0].cp);

    wchar_t* desc = nullptr;
    UINT     dLen = 0;
    if (!VerQueryValueW(buf.data(), subBlock, reinterpret_cast<void**>(&desc), &dLen) || !dLen)
        return L"";

    return std::wstring(desc, dLen - 1);
}

} // namespace

bool ActiveAppWatcher::Start(Callback callback)
{
    s_callback = callback;
    s_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    return s_hook != nullptr;
}

void ActiveAppWatcher::Stop()
{
    if (s_hook) { UnhookWinEvent(s_hook); s_hook = nullptr; }
}

void CALLBACK ActiveAppWatcher::WinEventProc(
    HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (!hwnd || idObject != OBJID_WINDOW || !s_callback) return;

    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);

    // Clicking our own bar/dock shouldn't change what's displayed.
    if (IsOwnOverlayClass(cls)) return;

    // Desktop / File Explorer / Start menu → no real app is focused, reset to "Dock".
    if (IsDesktopClass(cls)) { s_callback(L""); return; }

    std::wstring name = GetAppNameFromHwnd(hwnd);
    if (name.empty()) { s_callback(L""); return; }  // treat unresolved as "Dock"
    s_callback(name);
}

std::wstring ActiveAppWatcher::GetAppNameFromHwnd(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return L"";

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return L"";

    wchar_t path[MAX_PATH] = {};
    DWORD   len = MAX_PATH;
    QueryFullProcessImageNameW(hProc, 0, path, &len);
    CloseHandle(hProc);
    if (!path[0]) return L"";

    std::wstring exePath(path);
    if (IsShellExe(exePath)) return L"";

    auto& cache = NameCache();
    auto it = cache.find(exePath);
    if (it != cache.end()) return it->second;

    std::wstring name = FileDescription(exePath);
    if (name.empty()) name = ExeBasename(exePath);

    cache[exePath] = name;
    return name;
}
