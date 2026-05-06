// smoke_test.cpp
// Trivial Google Test case to verify the test pipeline (FetchContent,
// linker, gtest_discover_tests, ctest) is wired up end-to-end.
// Replace/extend with real unit tests as Phase 2+ components land.

#include <gtest/gtest.h>

TEST(Smoke, PipelineIsAlive)
{
    EXPECT_EQ(2 + 2, 4);
}
