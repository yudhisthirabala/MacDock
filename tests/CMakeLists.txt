# tests/CMakeLists.txt
# Automated unit tests (DEC-010 — Google Test via FetchContent).
# Pure-logic tests only. Win32 UI behavior stays in docs/TESTING.md as manual tests.

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# CRT alignment with the app (which uses /MT static CRT).
#
# History (Session 003):
#   - First try: `gtest_force_shared_crt OFF`. Ignored by googletest v1.14 —
#     it no longer honors that variable; left gtest compiled with /MD.
#   - This version: use CMake's standard abstraction `CMAKE_MSVC_RUNTIME_LIBRARY`
#     (needs policy CMP0091 NEW, default since CMake 3.15; we require 3.20).
#     Set before FetchContent_MakeAvailable so gtest targets pick it up at
#     configure time. Belt-and-suspenders: also force the property on gtest's
#     own targets after they exist, in case a future gtest version overrides
#     the global.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Keep this for older gtest versions that still honor it; harmless otherwise.
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Force the static-CRT property on gtest's own targets explicitly.
# Required because v1.14 does not wire gtest_force_shared_crt into anything.
if(MSVC)
    foreach(gt_target gtest gtest_main gmock gmock_main)
        if(TARGET ${gt_target})
            set_property(TARGET ${gt_target} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endforeach()
endif()

enable_testing()
include(GoogleTest)

# Smoke test — proves FetchContent, linker, and ctest pipeline all work.
# Real logic tests (ConfigManager, AppLauncher path validation, SystemInfo parsing)
# are added as each Phase 2+ component lands.
add_executable(macOSWin_tests
    smoke_test.cpp
    config_manager_test.cpp
    drop_handler_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/dock/DockDropValidator.cpp
)

target_include_directories(macOSWin_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(macOSWin_tests PRIVATE GTest::gtest_main)

if(MSVC)
    set_property(TARGET macOSWin_tests PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

gtest_discover_tests(macOSWin_tests)
