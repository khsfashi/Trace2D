#include "GpuQaFixtureOutcome.hpp"

#include <trace2d/platform/Platform.hpp>
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

TEST(GpuConformanceTests, StructuredFailureCategoryVocabularyIsStable)
{
    using trace2d::render::MaterialGpuPrepareError2D;
    using trace2d::test::CategoryForMaterialPrepareError;
    using trace2d::test::GpuQaFailureCategory;
    using trace2d::test::GpuQaFixtureOutcome;
    using trace2d::test::ToString;

    GpuQaFixtureOutcome outcome{
        "GpuConformanceTests.StructuredFailureCategoryVocabularyIsStable"};

    EXPECT_EQ(ToString(GpuQaFailureCategory::UnsupportedCapability), "unsupported_capability");
    EXPECT_EQ(
        ToString(GpuQaFailureCategory::GpuDeviceInitializationFailure),
        "gpu_device_initialization_failure");
    EXPECT_EQ(
        ToString(GpuQaFailureCategory::ShaderCompileOrReflectionFailure),
        "shader_compile_or_reflection_failure");
    EXPECT_EQ(
        ToString(GpuQaFailureCategory::PipelineOrResourceCreationFailure),
        "pipeline_or_resource_creation_failure");
    EXPECT_EQ(
        ToString(GpuQaFailureCategory::RenderSubmitOrDeviceLossFailure),
        "render_submit_or_device_loss_failure");
    EXPECT_EQ(
        ToString(GpuQaFailureCategory::ReadbackOrCaptureFailure),
        "readback_or_capture_failure");
    EXPECT_EQ(ToString(GpuQaFailureCategory::ComparisonMismatch), "comparison_mismatch");

    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::None),
        GpuQaFailureCategory::None);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::InvalidMaterialHandle),
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::InvalidShaderHandle),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::UnsupportedShaderLanguage),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::UnsupportedShaderStage),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::InvalidShaderEntryPoint),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::ParameterPreparationFailed),
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::ShaderCompilationFailed),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::ShaderReflectionFailed),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::ShaderContractMismatch),
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    EXPECT_EQ(
        CategoryForMaterialPrepareError(MaterialGpuPrepareError2D::PipelineCreationFailed),
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
}

TEST(GpuConformanceTests, StructuredOutcomeProbeTraversesRealGpuValidationPhases)
{
    constexpr std::string_view TestName =
        "GpuConformanceTests.StructuredOutcomeProbeTraversesRealGpuValidationPhases";

    using trace2d::test::GpuQaFailureCategory;
    using trace2d::test::GpuQaFixtureOutcome;
    GpuQaFixtureOutcome outcome{TestName};

    if (!GpuConformanceEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a maintained presentation-GPU environment.";
    }

    using namespace trace2d;

    outcome.SetFailurePoint(
        "device_initialization",
        GpuQaFailureCategory::GpuDeviceInitializationFailure);

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D GPUQA structured outcome probe";
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
    sprite.center = render::Float2{0.0F, 0.0F};
    sprite.halfExtents = render::Float2{0.5F, 0.5F};
    sprite.texture = texture;
    const render::OrthographicCamera camera{{0.0F, 0.0F}, 4.0F};

    outcome.SetFailurePoint(
        "render_submit",
        GpuQaFailureCategory::RenderSubmitOrDeviceLossFailure);
    renderer.RenderFrame(camera, sprite);

    const render::RenderMetrics beforeCapture = renderer.Metrics();
    EXPECT_EQ(beforeCapture.explicitGpuReadbacks, 0U);
    EXPECT_EQ(beforeCapture.explicitGpuFenceWaits, 0U);

    outcome.SetFailurePoint(
        "readback_capture",
        GpuQaFailureCategory::ReadbackOrCaptureFailure);
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "trace2d_gpuqa_structured_outcome.bmp";
    const render::CapturedFrame frame = renderer.CaptureFrame(
        render::CaptureRequest{9300U, path, render::CaptureImageFormat::Bmp},
        camera,
        sprite);

    ASSERT_EQ(frame.width, 64U);
    ASSERT_EQ(frame.height, 64U);
    ASSERT_EQ(frame.rgba8Pixels.size(), 64U * 64U * 4U);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    const std::size_t centerOffset = (32U * 64U + 32U) * 4U;
    EXPECT_GE(frame.rgba8Pixels[centerOffset], 248U);
    EXPECT_GE(frame.rgba8Pixels[centerOffset + 1U], 248U);
    EXPECT_GE(frame.rgba8Pixels[centerOffset + 2U], 248U);
    EXPECT_GE(frame.rgba8Pixels[centerOffset + 3U], 248U);

    const render::RenderMetrics afterCapture = renderer.Metrics();
    EXPECT_GT(afterCapture.explicitGpuReadbacks, beforeCapture.explicitGpuReadbacks);
    EXPECT_GT(afterCapture.explicitGpuFenceWaits, beforeCapture.explicitGpuFenceWaits);

    renderer.DestroyTexture(texture);
    RemoveCapture(path);
}
