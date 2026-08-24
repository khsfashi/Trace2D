#include "GpuQaFixtureOutcome.hpp"

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/CameraViewport2D.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string_view>

namespace
{
[[nodiscard]] bool GpuSmokeEnabled() noexcept
{
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t valueSize = 0U;
    if (_dupenv_s(&value, &valueSize, "TRACE2D_RUN_GPU_SMOKE") != 0 || value == nullptr)
    {
        return false;
    }
    const bool enabled = std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const char* const value = std::getenv("TRACE2D_RUN_GPU_SMOKE");
    return value != nullptr && std::string_view{value} == "1";
#endif
}

struct Rgba8 final
{
    std::uint8_t red{0U};
    std::uint8_t green{0U};
    std::uint8_t blue{0U};
    std::uint8_t alpha{0U};
};

[[nodiscard]] Rgba8 PixelAt(
    const trace2d::render::CapturedFrame& frame,
    const std::uint32_t x,
    const std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * frame.width + x) * 4U;
    return Rgba8{
        frame.rgba8Pixels[offset],
        frame.rgba8Pixels[offset + 1U],
        frame.rgba8Pixels[offset + 2U],
        frame.rgba8Pixels[offset + 3U],
    };
}

void ExpectNear(
    const Rgba8 actual,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue,
    const std::uint8_t alpha = 255U)
{
    constexpr int Tolerance = 8;
    EXPECT_NEAR(actual.red, red, Tolerance);
    EXPECT_NEAR(actual.green, green, Tolerance);
    EXPECT_NEAR(actual.blue, blue, Tolerance);
    EXPECT_NEAR(actual.alpha, alpha, Tolerance);
}

void RemoveFile(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::remove(path, error);
}
} // namespace

TEST(CameraViewportGpuConformanceTests, InterpolatedFitViewportMatchesCpuProjectionOnRealGpu)
{
    constexpr std::string_view TestName =
        "CameraViewportGpuConformanceTests.InterpolatedFitViewportMatchesCpuProjectionOnRealGpu";
    using trace2d::test::GpuQaFailureCategory;
    trace2d::test::GpuQaFixtureOutcome outcome{TestName};

    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a machine with a presentation GPU to run this test.";
    }

    using namespace trace2d;

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    render::Viewport2D viewport{};
    viewport.logicalWidth = 32U;
    viewport.logicalHeight = 16U;
    viewport.scaleMode = render::ViewportScaleMode2D::Fit;
    const render::ViewportResolveResult2D resolvedViewport =
        render::ResolveViewport2D(viewport, 64U, 64U);
    ASSERT_TRUE(resolvedViewport.Succeeded());
    EXPECT_EQ(resolvedViewport.viewport.contentRect.origin, (render::Float2{0.0F, 16.0F}));
    EXPECT_EQ(resolvedViewport.viewport.contentRect.size, (render::Float2{64.0F, 32.0F}));

    render::CameraFrameState2D previous{};
    previous.entity = scene::EntityId{7U, 1U};
    previous.center = render::Float2{2.0F, -2.0F};
    previous.verticalSize = 8.0F;

    render::CameraFrameState2D current{};
    current.entity = previous.entity;
    current.center = render::Float2{6.0F, 2.0F};
    current.verticalSize = 8.0F;

    const render::PresentationViewResult2D resolvedView = render::ResolvePresentationView2D(
        current,
        &previous,
        resolvedViewport.viewport,
        render::PresentationSamplingMode2D::Interpolated,
        0.5F);
    ASSERT_TRUE(resolvedView.Succeeded());
    EXPECT_EQ(resolvedView.view.rendererCamera.center, (render::Float2{4.0F, 0.0F}));

    const render::Float2 cpuPredictedCenter =
        render::WorldToPresentation(resolvedView.view, render::Float2{4.0F, 0.0F});
    EXPECT_NEAR(cpuPredictedCenter.x, 32.0F, 1.0e-5F);
    EXPECT_NEAR(cpuPredictedCenter.y, 32.0F, 1.0e-5F);

    outcome.SetFailurePoint(
        "device_initialization",
        GpuQaFailureCategory::GpuDeviceInitializationFailure);
    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D Camera/Viewport GPU conformance";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};

    outcome.SetFailurePoint(
        "pipeline_or_resource_creation",
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    constexpr std::array<std::uint8_t, 4U> WhitePixel{255U, 255U, 255U, 255U};
    const render::TextureHandle texture =
        renderer.CreateTextureRgba8(render::Rgba8TextureData{1U, 1U, WhitePixel});

    render::SpriteRenderData sprite{};
    sprite.center = render::Float2{4.0F, 0.0F};
    sprite.halfExtents = render::Float2{0.75F, 0.75F};
    sprite.texture = texture;

    outcome.SetFailurePoint(
        "render_submit",
        GpuQaFailureCategory::RenderSubmitOrDeviceLossFailure);
    renderer.RenderFrame(resolvedView.view.rendererCamera, sprite);
    const render::RenderMetrics beforeCapture = renderer.Metrics();
    EXPECT_EQ(beforeCapture.explicitGpuReadbacks, 0U);
    EXPECT_EQ(beforeCapture.explicitGpuFenceWaits, 0U);

    outcome.SetFailurePoint(
        "readback_capture",
        GpuQaFailureCategory::ReadbackOrCaptureFailure);
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "trace2d_camera_viewport_gpuqa2.bmp";
    const render::CapturedFrame frame = renderer.CaptureFrame(
        render::CaptureRequest{900U, path, render::CaptureImageFormat::Bmp},
        resolvedView.view.rendererCamera,
        sprite);
    ASSERT_EQ(frame.width, 64U);
    ASSERT_EQ(frame.height, 64U);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    // The CPU-resolved interpolated camera predicts this exact presentation center. The real GPU
    // must draw through that same Renderer camera, while Fit mode keeps the 16-pixel letterbox clear.
    ExpectNear(PixelAt(frame, 32U, 32U), 255U, 255U, 255U);
    ExpectNear(PixelAt(frame, 32U, 8U), 0U, 0U, 0U);
    ExpectNear(PixelAt(frame, 32U, 56U), 0U, 0U, 0U);

    const render::RenderMetrics afterCapture = renderer.Metrics();
    EXPECT_GT(afterCapture.explicitGpuReadbacks, beforeCapture.explicitGpuReadbacks);
    EXPECT_GT(afterCapture.explicitGpuFenceWaits, beforeCapture.explicitGpuFenceWaits);

    RemoveFile(path);
}
