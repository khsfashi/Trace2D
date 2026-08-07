#include <trace2d/platform/Platform.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(PlatformTests, HeadlessModeInitializesWithoutWindow)
{
    trace2d::platform::PlatformConfig config{};
    config.mode = trace2d::platform::StartupMode::Headless;

    trace2d::platform::Platform platform{config};

    EXPECT_EQ(platform.Mode(), trace2d::platform::StartupMode::Headless);
    EXPECT_FALSE(platform.HasWindow());
}

TEST(PlatformTests, StartupModeNamesAreStable)
{
    EXPECT_STREQ(trace2d::platform::ToString(trace2d::platform::StartupMode::Headless), "headless");
    EXPECT_STREQ(trace2d::platform::ToString(trace2d::platform::StartupMode::Windowed), "windowed");
}
} // namespace
