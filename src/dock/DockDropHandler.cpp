// DockDropHandler.cpp
// Drag-and-drop handling for the Dock (DEC-011).
// Accepts .exe and .lnk files; silently ignores duplicates and invalid types.

#include "DockDropHandler.h"
#include "DockDropValidator.h"
#include "DockWindow.h"
#include <shellapi.h>

int DockDropHandler::HandleDrop(HDROP hDrop, DockWindow* dock)
{
    int pinned = 0;
    const UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

    for (UINT i = 0; i < fileCount; ++i)
    {
        wchar_t path[MAX_PATH] = {};
        DragQueryFileW(hDrop, i, path, MAX_PATH);

        // DEC-011 sub-1: only .exe and .lnk accepted
        if (!DockDropValidator::IsValidAppFile(path))
            continue;

        // DEC-011 sub-3: silently ignore duplicates
        if (dock->HasIcon(path))
            continue;

        std::wstring name = DockDropValidator::ExtractAppName(path);
        dock->AddIcon(path, name);
        ++pinned;
    }

    DragFinish(hDrop);
    return pinned;
}
