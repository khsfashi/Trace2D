#include <trace2d/particles/ParticleProgram.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <array>
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

[[nodiscard]] trace2d::particles::ParticleEffectAsset MakeSmokeEffect()
{
    using namespace trace2d::particles;

    ParticleEffectAsset effect{};
    effect.id = "effects/gpu_smoke.trace2d.particle.toml";
    effect.semanticId = "gpu_smoke";
    effect.backend = ParticleEffectBackend::Gpu;
    effect.lifecycle.durationFrames = 8U;
    effect.lifecycle.loop = false;
    effect.lifecycle.playOnLoad = true;
    effect.definition.maxParticles = 16U;
    effect.definition.lifetimeFrames = {4U, 4U};
    effect.definition.initialSize = {0.5F, 0.5F};
    effect.definition.endSizeMultiplier = 0.5F;
    effect.definition.initialColor.minValue = {1.0F, 1.0F, 1.0F, 1.0F};
    effect.definition.initialColor.maxValue = effect.definition.initialColor.minValue;
    effect.definition.endColor = {1.0F, 1.0F, 1.0F, 0.0F};
    effect.definition.spriteChoiceCount = 1U;
    effect.spriteReferences = {"textures/gpu_smoke.png"};
    effect.bursts.push_back(ParticleBurst{0U, 4U});
    return effect;
}

TEST(ParticleGpuSmokeTests, ExplicitGpuEmitterAdvancesCapturesAndReusesCapacity)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a machine with a presentation GPU to run this test.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D GPU particle smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};

    constexpr std::array<std::uint8_t, 16> WhiteTexture{
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
    };
    const render::TextureHandle texture = renderer.CreateTextureRgba8(
        render::Rgba8TextureData{2U, 2U, WhiteTexture});

    const particles::ParticleProgram program =
        particles::CompileParticleProgram(MakeSmokeEffect());
    const particles::ParticleGpuCompileResult artifact =
        particles::CompileParticleGpuArtifact(program);
    ASSERT_TRUE(artifact.Ok());

    const render::GpuParticleEmitterCreateResult created =
        renderer.CreateGpuParticleEmitter(program, 0x12345678ULL, 42U, texture);
    ASSERT_TRUE(created.Ok());
    ASSERT_NE(created.handle, render::InvalidGpuParticleEmitterHandle);
    EXPECT_EQ(created.support.strideBytes, artifact.artifact.strideBytes);
    EXPECT_EQ(created.support.particleBufferBytes, artifact.artifact.bufferBytes);

    ASSERT_TRUE(renderer.StepGpuParticleEmitter(
        render::GpuParticleStepData{created.handle, {0.0F, 0.0F}}));

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 4.0F};
    const render::GpuParticleRenderData particleRender{
        created.handle,
        {0.0F, 0.0F},
        0,
        0U,
    };

    const std::filesystem::path capturePath =
        std::filesystem::temp_directory_path() / "trace2d_gpu_particle_smoke.bmp";
    const render::CaptureRequest captureRequest{0U, capturePath, render::CaptureImageFormat::Bmp};
    const render::CapturedFrame captured = renderer.CaptureFrame(
        captureRequest,
        camera,
        {},
        std::span<const render::GpuParticleRenderData>{&particleRender, 1U});

    EXPECT_EQ(captured.simulationFrame, 0U);
    EXPECT_EQ(captured.width, 64U);
    EXPECT_EQ(captured.height, 64U);
    EXPECT_FALSE(captured.rgba8Pixels.empty());

    render::GpuParticleEmitterMetrics metrics = renderer.GpuParticleMetrics(created.handle);
    EXPECT_EQ(metrics.particleBufferCreations, 1U);
    EXPECT_EQ(metrics.particleBufferGrowths, 0U);
    EXPECT_EQ(metrics.submittedSteps, 1U);
    EXPECT_EQ(metrics.submittedSpawnAttempts, 4U);
    EXPECT_EQ(metrics.spawnDispatches, 1U);
    EXPECT_EQ(metrics.renderDraws, 1U);
    EXPECT_EQ(metrics.renderInstances, 4U);
    EXPECT_EQ(metrics.instanceUpperBound, 4U);
    EXPECT_EQ(metrics.normalFrameReadbacks, 0U);
    EXPECT_EQ(metrics.normalFrameFenceWaits, 0U);

    ASSERT_TRUE(renderer.StepGpuParticleEmitter(
        render::GpuParticleStepData{created.handle, {0.0F, 0.0F}}));
    metrics = renderer.GpuParticleMetrics(created.handle);
    EXPECT_EQ(metrics.particleBufferCreations, 1U);
    EXPECT_EQ(metrics.particleBufferGrowths, 0U);
    EXPECT_EQ(metrics.submittedSteps, 2U);

    const render::RenderMetrics& renderMetrics = renderer.Metrics();
    EXPECT_EQ(renderMetrics.gpuParticleDrawCalls, 1U);
    EXPECT_EQ(renderMetrics.submittedGpuParticleInstances, 4U);

    renderer.DestroyGpuParticleEmitter(created.handle);
    renderer.DestroyTexture(texture);
    std::error_code removeError{};
    std::filesystem::remove(capturePath, removeError);
}
} // namespace
