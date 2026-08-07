#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
TEST(RendererTests, HeadlessPlatformHasNoWindowIdentifier)
{
    trace2d::platform::PlatformConfig platformConfig{};
    platformConfig.mode = trace2d::platform::StartupMode::Headless;

    const trace2d::platform::Platform platform{platformConfig};

    EXPECT_FALSE(platform.HasWindow());
    EXPECT_EQ(platform.WindowIdValue(), trace2d::platform::InvalidWindowId);
}

TEST(RendererTests, RendererRejectsHeadlessPlatformBeforeGpuInitialization)
{
    trace2d::platform::PlatformConfig platformConfig{};
    platformConfig.mode = trace2d::platform::StartupMode::Headless;

    const trace2d::platform::Platform platform{platformConfig};
    const trace2d::render::RendererConfig rendererConfig{};

    EXPECT_THROW(
        static_cast<void>(trace2d::render::Renderer(rendererConfig, platform)),
        std::invalid_argument);
}
} // namespace
