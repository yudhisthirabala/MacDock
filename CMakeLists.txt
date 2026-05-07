cmake_minimum_required(VERSION 3.20)
project(macOSWin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-only project
if(NOT WIN32)
    message(FATAL_ERROR "This project only supports Windows.")
endif()

# Output a single .exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── nlohmann/json ──────────────────────────────────────────────────────────────
# Auto-downloaded at configure time via FetchContent — no manual setup required.
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR        ${CMAKE_SOURCE_DIR}/vendor/nlohmann
)
FetchContent_MakeAvailable(nlohmann_json)
include_directories(vendor)

# Source files
set(SOURCES
    src/main.cpp

    src/dock/DockWindow.cpp
    src/dock/DockIcon.cpp
    src/dock/DockDropTarget.cpp
    src/dock/DockDropValidator.cpp

    src/menubar/MenuBarWindow.cpp
    src/menubar/ActiveAppWatcher.cpp
    src/menubar/SystemInfoBar.cpp
    src/menubar/FlyoutWindow.cpp

    src/system/TaskbarManager.cpp
    src/system/TrayIcon.cpp
    src/system/AppBarManager.cpp
    src/system/ProcessMonitor.cpp
    src/system/AppLauncher.cpp
    src/system/SystemInfo.cpp
    src/system/CrashRecovery.cpp

    src/config/ConfigManager.cpp
)

# Build as Windows GUI app (no console window)
add_executable(macOSWin WIN32 ${SOURCES})

# Link required Windows libraries
target_link_libraries(macOSWin
    d2d1            # Direct2D
    dwrite          # DirectWrite (text rendering)
    dwmapi          # Desktop Window Manager
    shell32         # ShellExecute, drag-drop, icon extraction
    ole32           # COM initialization
    oleaut32        # COM automation
    wlanapi         # Wi-Fi APIs
    winmm           # Multimedia (timer)
    psapi           # Process information (GetModuleFileNameEx)
    uxtheme         # Windows theming
    msimg32         # AlphaBlend
    gdiplus         # High-quality bicubic icon scaling
    version         # Version info resource (ActiveAppWatcher FileDescription lookup)
    windowsapp      # C++/WinRT (SMTC playback state for MenuBarWindow)
    advapi32        # Registry API (DEC-029 startup toggle)
)

# Compiler options
if(MSVC)
    target_compile_options(macOSWin PRIVATE /W4 /WX- /utf-8)
    # Enable static CRT for self-contained exe
    set_property(TARGET macOSWin PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── Tests (DEC-010) ────────────────────────────────────────────────────────────
# Opt-in via -DBUILD_TESTS=ON. Off by default so release builds of the .exe
# don't need to download Google Test.
option(BUILD_TESTS "Build automated unit tests (Google Test)" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "macOSWin configured. Build with: cmake --build . --config Release")
