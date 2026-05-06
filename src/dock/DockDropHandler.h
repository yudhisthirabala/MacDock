// DockDropHandler.h
// Handles drag-and-drop of .exe and .lnk files onto the Dock (DEC-011).
// Validation logic lives in DockDropValidator for testability.

#pragma once
#include <windows.h>

class DockWindow; // forward declaration

class DockDropHandler
{
public:
    // Process a WM_DROPFILES message. Validates each file and pins valid ones.
    // Returns the count of files that were actually pinned (0 if all rejected).
    static int HandleDrop(HDROP hDrop, DockWindow* dock);
};
