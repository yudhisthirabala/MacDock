// DockDropTarget.cpp
// OLE IDropTarget for reliable drag-and-drop on the Dock window.
// Supports files from Explorer (CF_HDROP) and apps from the Start Menu
// (Shell IDList Array → IShellItem → IShellLink resolution).

#include "DockDropTarget.h"
#include "DockDropValidator.h"
#include "DockWindow.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <algorithm>

// Clipboard format for shell ID lists (registered at runtime)
static CLIPFORMAT GetShellIdListFormat()
{
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(
        RegisterClipboardFormatW(L"Shell IDList Array"));
    return cf;
}

DockDropTarget::DockDropTarget(DockWindow* dock)
    : m_refCount(1), m_dock(dock), m_hasValidData(false)
{
}

// ── IUnknown ────────────────────────────────────────────────────────────────

HRESULT STDMETHODCALLTYPE DockDropTarget::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IDropTarget)
    {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DockDropTarget::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE DockDropTarget::Release()
{
    LONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) delete this;
    return ref;
}

// ── IDropTarget ─────────────────────────────────────────────────────────────

HRESULT STDMETHODCALLTYPE DockDropTarget::DragEnter(
    IDataObject* pDataObj, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect)
{
    m_hasValidData = HasFileData(pDataObj);
    if (!m_hasValidData)
        *pdwEffect = DROPEFFECT_NONE;
    // else: keep whatever the source offers (COPY/MOVE/LINK)
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DockDropTarget::DragOver(
    DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect)
{
    if (!m_hasValidData)
        *pdwEffect = DROPEFFECT_NONE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DockDropTarget::DragLeave()
{
    m_hasValidData = false;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DockDropTarget::Drop(
    IDataObject* pDataObj, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect)
{
    *pdwEffect = DROPEFFECT_NONE;

    auto files = ExtractFilePaths(pDataObj);
    if (files.empty())
        return S_OK;

    int pinned = 0;
    for (const auto& path : files)
    {
        if (!DockDropValidator::IsValidAppFile(path))
            continue;

        if (m_dock->HasIcon(path))
            continue;

        std::wstring name = DockDropValidator::ExtractAppName(path);
        m_dock->AddIcon(path, name);
        ++pinned;
    }

    m_dock->OnDropComplete(pinned, static_cast<int>(files.size()));

    if (pinned > 0)
        *pdwEffect = DROPEFFECT_LINK;
    return S_OK;
}

// ── Helpers ─────────────────────────────────────────────────────────────────

bool DockDropTarget::HasFileData(IDataObject* pDataObj)
{
    FORMATETC fmtHdrop = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (pDataObj->QueryGetData(&fmtHdrop) == S_OK)
        return true;

    FORMATETC fmtShell = { GetShellIdListFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (pDataObj->QueryGetData(&fmtShell) == S_OK)
        return true;

    return false;
}

// Returns true if a path looks like a real filesystem path (has a drive letter
// or UNC prefix, not just an app ID like "MSEdge")
static bool LooksLikeRealPath(const std::wstring& path)
{
    if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
        return true; // e.g. C:\...
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
        return true; // UNC path
    return false;
}

// Extract file paths from CF_HDROP data (Explorer file drags)
static std::vector<std::wstring> ExtractFromHDROP(IDataObject* pDataObj)
{
    std::vector<std::wstring> paths;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg)))
        return paths;

    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (hDrop)
    {
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            wchar_t path[MAX_PATH] = {};
            DragQueryFileW(hDrop, i, path, MAX_PATH);
            // Start Menu CF_HDROP contains app IDs ("MSEdge") not real paths.
            // Only accept entries that look like actual filesystem paths.
            if (LooksLikeRealPath(path))
                paths.emplace_back(path);
        }
        GlobalUnlock(stg.hGlobal);
    }

    ReleaseStgMedium(&stg);
    return paths;
}

// Helper: get the absolute PIDL for item i in a CIDA structure
static PIDLIST_ABSOLUTE GetAbsolutePidl(const CIDA* pCida, UINT index)
{
    LPCITEMIDLIST pidlFolder = reinterpret_cast<LPCITEMIDLIST>(
        reinterpret_cast<const BYTE*>(pCida) + pCida->aoffset[0]);
    LPCITEMIDLIST pidlChild = reinterpret_cast<LPCITEMIDLIST>(
        reinterpret_cast<const BYTE*>(pCida) + pCida->aoffset[index + 1]);
    return ILCombine(pidlFolder, pidlChild);
}

// Resolve a shell item to its .lnk shortcut path or target .exe path
static std::wstring ResolveShellItem(PIDLIST_ABSOLUTE pidlAbs)
{
    // Method 1: SHGetPathFromIDList — works for filesystem items
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidlAbs, path) && path[0] != L'\0')
    {
        if (LooksLikeRealPath(path))
            return path;
    }

    // Method 2: IShellItem → filesystem path
    IShellItem* pItem = nullptr;
    if (SUCCEEDED(SHCreateItemFromIDList(pidlAbs, IID_PPV_ARGS(&pItem))))
    {
        LPWSTR fsPath = nullptr;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &fsPath)) && fsPath)
        {
            std::wstring result(fsPath);
            CoTaskMemFree(fsPath);
            if (LooksLikeRealPath(result))
            {
                pItem->Release();
                return result;
            }
        }

        // Method 3: IShellItem → IShellLink → resolve shortcut target
        IShellLinkW* pLink = nullptr;
        if (SUCCEEDED(pItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pLink))))
        {
            wchar_t target[MAX_PATH] = {};
            if (SUCCEEDED(pLink->GetPath(target, MAX_PATH, nullptr, 0)) && target[0] != L'\0')
            {
                pLink->Release();
                pItem->Release();
                return target;
            }
            pLink->Release();
        }

        // Method 4 (UWP / Start Menu apps): SIGDN_DESKTOPABSOLUTEPARSING gives
        // a parsing name like "shell:AppsFolder\Microsoft.WindowsStickyNotes_…!App"
        // which ShellExecute can launch and AddIcon can extract a tile from.
        LPWSTR parsing = nullptr;
        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parsing)) && parsing)
        {
            std::wstring result(parsing);
            CoTaskMemFree(parsing);
            pItem->Release();
            // Wrap any non-filesystem parsing name in shell:AppsFolder\.
            if (LooksLikeRealPath(result)) return result;
            return std::wstring(L"shell:AppsFolder\\") + result;
        }

        pItem->Release();
    }

    return {};
}

// Resolve a .lnk shortcut to its target exe filename (lowercase, no path)
static std::wstring ResolveLnkTargetExeName(const std::wstring& lnkPath)
{
    IShellLinkW* pLink = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&pLink))))
        return {};

    IPersistFile* pFile = nullptr;
    if (FAILED(pLink->QueryInterface(IID_PPV_ARGS(&pFile))))
    { pLink->Release(); return {}; }

    if (FAILED(pFile->Load(lnkPath.c_str(), STGM_READ)))
    { pFile->Release(); pLink->Release(); return {}; }

    wchar_t target[MAX_PATH] = {};
    pLink->GetPath(target, MAX_PATH, nullptr, 0);
    pFile->Release();
    pLink->Release();

    std::wstring targetStr(target);
    if (targetStr.empty()) return {};

    // Extract just the exe filename, lowercase
    size_t slash = targetStr.find_last_of(L"\\/");
    std::wstring exeName = (slash != std::wstring::npos)
        ? targetStr.substr(slash + 1) : targetStr;
    std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);
    return exeName;
}

// Scan a folder for .lnk files, call visitor(lnkFullPath) for each.
// Returns the first path for which visitor returns true, or empty string.
template<typename Fn>
static std::wstring ScanLnkFiles(const std::wstring& folder, Fn visitor)
{
    // Scan .lnk files in this folder
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW((folder + L"\\*.lnk").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            std::wstring lnkPath = folder + L"\\" + fd.cFileName;
            if (visitor(lnkPath))
            { FindClose(hFind); return lnkPath; }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    // Scan one level of subdirectories
    hFind = FindFirstFileW((folder + L"\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            std::wstring subFolder = folder + L"\\" + fd.cFileName;
            WIN32_FIND_DATAW fd2 = {};
            HANDLE hFind2 = FindFirstFileW((subFolder + L"\\*.lnk").c_str(), &fd2);
            if (hFind2 != INVALID_HANDLE_VALUE)
            {
                do {
                    std::wstring lnkPath = subFolder + L"\\" + fd2.cFileName;
                    if (visitor(lnkPath))
                    { FindClose(hFind2); FindClose(hFind); return lnkPath; }
                } while (FindNextFileW(hFind2, &fd2));
                FindClose(hFind2);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    return {};
}

// Words too generic to use for token-based matching (Match 3).
// Without this, "windows.immersivecontrolpanel..." matches any .lnk whose
// target contains "windows" in its name.
static bool IsGenericToken(const std::wstring& tok)
{
    static const wchar_t* const kGeneric[] = {
        L"microsoft", L"windows", L"app", L"exe", L"com", L"net",
        L"the", L"new", L"pro", L"desktop",
    };
    for (const auto* g : kGeneric)
        if (tok == g) return true;
    return false;
}

// Search Start Menu folders for a .lnk whose target exe matches the app ID.
// App IDs from the Start Menu look like "MSEdge", "Microsoft.Office.WINWORD.EXE.15", etc.
static std::wstring FindStartMenuShortcut(const std::wstring& appId)
{
    wchar_t commonStartMenu[MAX_PATH] = {};
    wchar_t userStartMenu[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, commonStartMenu);
    SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, userStartMenu);

    std::wstring appIdLower = appId;
    std::transform(appIdLower.begin(), appIdLower.end(), appIdLower.begin(), ::towlower);

    // Tokenize on dots, underscores, and '!' — these are separators in app IDs
    // "Microsoft.Office.WINWORD.EXE.15" → [microsoft, office, winword, exe, 15]
    // "Microsoft.WindowsStore_8wekyb3d8bbwe!App" → [microsoft, windowsstore, 8wekyb3d8bbwe, app]
    std::vector<std::wstring> appIdTokens;
    {
        std::wstring token;
        for (wchar_t c : appIdLower)
        {
            if (c == L'.' || c == L'_' || c == L'!')
            {
                if (!token.empty()) appIdTokens.push_back(token);
                token.clear();
            }
            else
            {
                token += c;
            }
        }
        if (!token.empty()) appIdTokens.push_back(token);
    }

    auto matcher = [&](const std::wstring& lnkPath) -> bool
    {
        std::wstring exeName = ResolveLnkTargetExeName(lnkPath);
        if (exeName.empty()) return false;

        // Strip .exe suffix
        std::wstring exeBase = exeName;
        if (exeBase.size() > 4 && exeBase.substr(exeBase.size() - 4) == L".exe")
            exeBase = exeBase.substr(0, exeBase.size() - 4);

        // Match 1: app ID contains the exe base name (must be 4+ chars to avoid false positives)
        if (exeBase.size() >= 4 && appIdLower.find(exeBase) != std::wstring::npos)
            return true;

        // Match 2: a non-generic app ID token exactly matches the exe base name
        for (const auto& tok : appIdTokens)
        {
            if (tok.size() >= 4 && !IsGenericToken(tok) && exeBase == tok)
                return true;
        }

        // Match 3: shortcut filename (no spaces) matches app ID
        // "Microsoft Edge.lnk" → "microsoftedge" vs appId containing "msedge"
        size_t slash = lnkPath.find_last_of(L"\\/");
        std::wstring lnkName = (slash != std::wstring::npos)
            ? lnkPath.substr(slash + 1) : lnkPath;
        std::transform(lnkName.begin(), lnkName.end(), lnkName.begin(), ::towlower);
        if (lnkName.size() > 4) lnkName = lnkName.substr(0, lnkName.size() - 4);

        std::wstring lnkCompact;
        for (wchar_t c : lnkName)
            if (c != L' ') lnkCompact += c;

        // Only match if the compact lnk name is reasonably specific (6+ chars)
        if (lnkCompact.size() >= 6)
        {
            if (appIdLower.find(lnkCompact) != std::wstring::npos)
                return true;
        }

        return false;
    };

    std::wstring result = ScanLnkFiles(commonStartMenu, matcher);
    if (!result.empty()) return result;

    result = ScanLnkFiles(userStartMenu, matcher);
    return result;
}

// Extract file paths from CFSTR_SHELLIDLIST (Start Menu, shell sources)
static std::vector<std::wstring> ExtractFromShellIdList(IDataObject* pDataObj)
{
    std::vector<std::wstring> paths;

    FORMATETC fmt = { GetShellIdListFormat(), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg)))
        return paths;

    const CIDA* pCida = static_cast<const CIDA*>(GlobalLock(stg.hGlobal));
    if (pCida)
    {
        for (UINT i = 0; i < pCida->cidl; ++i)
        {
            PIDLIST_ABSOLUTE pidlAbs = GetAbsolutePidl(pCida, i);
            if (!pidlAbs) continue;

            std::wstring resolved = ResolveShellItem(pidlAbs);
            // Accept either a real filesystem path or a shell:AppsFolder\ path
            // (the validator handles both).
            if (!resolved.empty())
            {
                paths.emplace_back(resolved);
            }
            ILFree(pidlAbs);
        }
        GlobalUnlock(stg.hGlobal);
    }

    ReleaseStgMedium(&stg);
    return paths;
}

// Try CF_HDROP for app IDs that failed filesystem resolution —
// search Start Menu for matching .lnk shortcuts
static std::vector<std::wstring> ExtractAppIdsAsShortcuts(IDataObject* pDataObj)
{
    std::vector<std::wstring> paths;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg)))
        return paths;

    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (hDrop)
    {
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            wchar_t buf[MAX_PATH] = {};
            DragQueryFileW(hDrop, i, buf, MAX_PATH);

            // If it's already a real path, skip (handled by ExtractFromHDROP)
            if (LooksLikeRealPath(buf))
                continue;

            // It's an app ID — try to find a matching Start Menu shortcut
            std::wstring resolved = FindStartMenuShortcut(buf);

            // If no .lnk found, it's likely a UWP/Store app — use shell:AppsFolder
            // which ShellExecute can launch and IShellItem can extract icons from
            if (resolved.empty())
                resolved = std::wstring(L"shell:AppsFolder\\") + buf;

            paths.emplace_back(resolved);
        }
        GlobalUnlock(stg.hGlobal);
    }

    ReleaseStgMedium(&stg);
    return paths;
}

std::vector<std::wstring> DockDropTarget::ExtractFilePaths(IDataObject* pDataObj)
{
    // 1. Try CF_HDROP for real filesystem paths (Explorer drags)
    auto paths = ExtractFromHDROP(pDataObj);
    if (!paths.empty())
        return paths;

    // 2. Try Shell IDList Array (shell items with PIDLs)
    paths = ExtractFromShellIdList(pDataObj);
    if (!paths.empty())
        return paths;

    // 3. Last resort: CF_HDROP contained app IDs — search Start Menu
    paths = ExtractAppIdsAsShortcuts(pDataObj);
    return paths;
}
