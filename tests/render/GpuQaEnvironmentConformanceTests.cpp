#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>

namespace
{
[[nodiscard]] bool GpuConformanceEnabled() noexcept
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

void RemoveCapture(const std::filesystem::path& path) noexcept
{
    std::error_code error{};
    std::filesystem::remove(path, error);
}
} // namespace

TEST(GpuConformanceTests, ReportsActualBackendAndExplicitCaptureBoundary)
{
    if (!GpuConformanceEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a maintained presentation-GPU environment.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D GPUQA environment conformance";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};

    ASSERT_FALSE(renderer.DriverName().empty());

    const render::RenderMetrics beforeCapture = renderer.Metrics();
    EXPECT_EQ(beforeCapture.explicitGpuReadbacks, 0U);
    EXPECT_EQ(beforeCapture.explicitGpuFenceWaits, 0U);

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 4.0F};
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "trace2d_gpuqa_environment.bmp";
    const render::CapturedFrame frame = renderer.CaptureFrame(
        render::CaptureRequest{9200U, path, render::CaptureImageFormat::Bmp},
        camera,
        std::span<const render::SpriteRenderData>{});

    ASSERT_EQ(frame.width, 64U);
    ASSERT_EQ(frame.height, 64U);
    ASSERT_EQ(frame.rgba8Pixels.size(), 64U * 64U * 4U);

    const render::RenderMetrics afterCapture = renderer.Metrics();
    EXPECT_GT(afterCapture.explicitGpuReadbacks, beforeCapture.explicitGpuReadbacks);
    EXPECT_GT(afterCapture.explicitGpuFenceWaits, beforeCapture.explicitGpuFenceWaits);

    std::cout
        << "TRACE2D_GPUQA_ENV_V1"
        << " backend=" << renderer.DriverName()
        << " viewport_width=" << frame.width
        << " viewport_height=" << frame.height
        << " capture_format=rgba8"
        << " comparison=metadata_exact"
        << '\n';

    RemoveCapture(path);
}
