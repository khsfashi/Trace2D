#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
TEST(RendererTests, RenderMetricsDefaultToZero)
{
    const trace2d::render::RenderMetrics metrics{};

    EXPECT_EQ(metrics.submittedFrames, 0U);
    EXPECT_EQ(metrics.presentedFrames, 0U);
    EXPECT_EQ(metrics.renderPasses, 0U);
    EXPECT_EQ(metrics.drawCalls, 0U);
    EXPECT_EQ(metrics.submittedSprites, 0U);
    EXPECT_EQ(metrics.submittedGpuParticleInstances, 0U);
    EXPECT_EQ(metrics.gpuParticleDrawCalls, 0U);
    EXPECT_EQ(metrics.culledSprites, 0U);
    EXPECT_EQ(metrics.spritePresentationDrawCalls, 0U);
    EXPECT_EQ(metrics.spritePresentationSprites, 0U);
    EXPECT_EQ(metrics.spriteSamplerCreations, 0U);
    EXPECT_EQ(metrics.spritePipelineCreations, 0U);
    EXPECT_EQ(metrics.spriteVertexCapacitySprites, 0U);
    EXPECT_EQ(metrics.explicitGpuReadbacks, 0U);
    EXPECT_EQ(metrics.explicitGpuFenceWaits, 0U);
    EXPECT_EQ(metrics.lastTargetWidth, 0U);
    EXPECT_EQ(metrics.lastTargetHeight, 0U);
}

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