// DockDropValidator.h
// Pure-logic validation and name extraction for drag-and-drop files (DEC-011).
// Separated from DockDropHandler so these functions can be unit-tested
// without pulling in DockWindow / Win32 UI dependencies.

#pragma once
#include <string>

namespace DockDropValidator {

// Returns true if the file has a pinnable extension (.exe or .lnk)
bool IsValidAppFile(const std::wstring& path);

// Extracts display name from a file path (filename without extension)
std::wstring ExtractAppName(const std::wstring& path);

} // namespace DockDropValidator
