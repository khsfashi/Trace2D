#include <trace2d/particles/ParticleProgram.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
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

struct BrightBounds final
{
    std::uint32_t minX{0};
    std::uint32_t maxX{0};
    std::uint32_t minY{0};
    std::uint32_t maxY{0};
    std::uint32_t pixelCount{0};
};

[[nodiscard]] bool TryFindBrightBounds(
    const trace2d::render::CapturedFrame& frame,
    BrightBounds& outBounds) noexcept
{
    constexpr std::uint8_t BrightThreshold = 200U;
    const std::uint64_t expectedBytes =
        static_cast<std::uint64_t>(frame.width) * frame.height * 4U;
    if (frame.width == 0U || frame.height == 0U ||
        expectedBytes != frame.rgba8Pixels.size())
    {
        return false;
    }

    BrightBounds bounds{};
    bounds.minX = std::numeric_limits<std::uint32_t>::max();
    bounds.minY = std::numeric_limits<std::uint32_t>::max();

    for (std::uint32_t y = 0U; y < frame.height; ++y)
    {
        for (std::uint32_t x = 0U; x < frame.width; ++x)
        {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * frame.width + x) * 4U;
            if (frame.rgba8Pixels[offset] < BrightThreshold ||
                frame.rgba8Pixels[offset + 1U] < BrightThreshold ||
                frame.rgba8Pixels[offset + 2U] < BrightThreshold)
            {
                continue;
            }

            bounds.minX = std::min(bounds.minX, x);
            bounds.maxX = std::max(bounds.maxX, x);
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxY = std::max(bounds.maxY, y);
            ++bounds.pixelCount;
        }
    }

    if (bounds.pixelCount == 0U)
    {
        return false;
    }

    outBounds = bounds;
    return true;
}

[[nodiscard]] float PixelCenterX(const BrightBounds& bounds) noexcept
{
    return (static_cast<float>(bounds.minX) +
            static_cast<float>(bounds.maxX) + 1.0F) * 0.5F;
}

[[nodiscard]] float ExpectedPixelCenterX(
    const trace2d::render::CapturedFrame& frame,
    const trace2d::render::OrthographicCamera& camera,
    const trace2d::particles::ParticleReferenceParticle& particle)
{
    trace2d::render::OrthographicView view{};
    if (!trace2d::render::TryBuildOrthographicView(
            camera, frame.width, frame.height, view))
    {
        throw std::runtime_error{"Failed to build conformance capture view."};
    }

    const trace2d::render::Float2 clip = trace2d::render::WorldToClip(
        view, {particle.position.x, particle.position.y});
    return ((clip.x * 0.5F) + 0.5F) * static_cast<float>(frame.width);
}

[[nodiscard]] trace2d::particles::ParticleEffectAsset MakeConformanceEffect()
{
    using namespace trace2d::particles;

    ParticleEffectAsset effect{};
    effect.id = "effects/gpu_conformance.trace2d.particle.toml";
    effect.semanticId = "gpu_conformance";
    effect.backend = ParticleEffectBackend::Gpu;
    effect.lifecycle.durationFrames = 16U;
    effect.lifecycle.loop = false;
    effect.lifecycle.playOnLoad = true;
    effect.definition.maxParticles = 1U;
    effect.definition.spawnShape.type = ParticleSpawnShapeType::Box;
    effect.definition.spawnShape.boxHalfExtents = {0.5F, 0.25F};
    effect.definition.lifetimeFrames = {3U, 5U};
    effect.definition.speed = {0.25F, 0.25F};
    effect.definition.angleRadians = {0.0F, 0.0F};
    effect.definition.initialSize = {0.4F, 0.4F};
    effect.definition.endSizeMultiplier = 1.0F;
    effect.definition.initialColor.minValue = {1.0F, 1.0F, 1.0F, 1.0F};
    effect.definition.initialColor.maxValue = effect.definition.initialColor.minValue;
    effect.definition.endColor = effect.definition.initialColor.minValue;
    effect.definition.spriteChoiceCount = 1U;
    effect.spriteReferences = {"textures/gpu_conformance.png"};
    effect.bursts.push_back(ParticleBurst{0U, 1U});
    return effect;
}

void RemoveCapture(const std::filesystem::path& path) noexcept
{
    std::error_code error{};
    std::filesystem::remove(path, error);
}

TEST(ParticleGpuConformanceTests, ExplicitGpuExecutionTracksCpuOracleAcrossRandomSpawnMotionAndLifetime)
{
    if (!GpuConformanceEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a machine with a presentation GPU to run conformance.";
    }

    using namespace trace2d;

    constexpr std::uint64_t GlobalSeed = 0x0123456789ABCDEFULL;
    constexpr particles::ParticleEmitterStableId StableId = 0x1122334455667788ULL;
    constexpr std::array<std::uint8_t, 16> WhiteTexture{
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
    };

    const particles::ParticleProgram program =
        particles::CompileParticleProgram(MakeConformanceEffect());
    const particles::ParticleGpuCompileResult artifact =
        particles::CompileParticleGpuArtifact(program);
    ASSERT_TRUE(artifact.Ok());

    particles::ParticleEmitter2D cpuOracle{};
    const particles::ParticleEmitter2DPrepareResult cpuPrepared =
        particles::PrepareParticleProgramCpuEmitter(
            program, GlobalSeed, StableId, cpuOracle);
    ASSERT_TRUE(cpuPrepared.Ok());

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 128;
    platformConfig.windowHeight = 128;
    platformConfig.windowTitle = "Trace2D GPU particle conformance";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};
    const render::TextureHandle texture = renderer.CreateTextureRgba8(
        render::Rgba8TextureData{2U, 2U, WhiteTexture});

    const render::GpuParticleEmitterCreateResult created =
        renderer.CreateGpuParticleEmitter(program, GlobalSeed, StableId, texture);
    ASSERT_TRUE(created.Ok());
    EXPECT_EQ(created.support.programFingerprint, program.fingerprint);
    EXPECT_EQ(created.support.artifactFingerprint, artifact.artifact.artifactFingerprint);
    EXPECT_EQ(created.support.strideBytes, artifact.artifact.strideBytes);
    EXPECT_EQ(created.support.particleBufferBytes, artifact.artifact.bufferBytes);

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 4.0F};
    const render::GpuParticleStepData step{created.handle, {0.0F, 0.0F}};
    const render::GpuParticleRenderData draw{created.handle, {0.0F, 0.0F}, 0, 0U};

    ASSERT_TRUE(cpuOracle.Step());
    ASSERT_TRUE(renderer.StepGpuParticleEmitter(step));
    ASSERT_EQ(cpuOracle.Reference().AliveCount(), 1U);

    particles::ParticleReferenceParticle cpuParticle{};
    ASSERT_TRUE(cpuOracle.Reference().TryGetParticle(0U, cpuParticle));
    const std::uint32_t sampledLifetime = cpuParticle.lifetimeFrames;
    ASSERT_GE(sampledLifetime, 3U);
    ASSERT_LE(sampledLifetime, 5U);

    const std::filesystem::path firstCapturePath =
        std::filesystem::temp_directory_path() / "trace2d_gpu_particle_conformance_0.bmp";
    const render::CapturedFrame firstCapture = renderer.CaptureFrame(
        render::CaptureRequest{0U, firstCapturePath, render::CaptureImageFormat::Bmp},
        camera,
        {},
        std::span<const render::GpuParticleRenderData>{&draw, 1U});

    BrightBounds firstBounds{};
    ASSERT_TRUE(TryFindBrightBounds(firstCapture, firstBounds));
    EXPECT_NEAR(
        PixelCenterX(firstBounds),
        ExpectedPixelCenterX(firstCapture, camera, cpuParticle),
        2.0F);

    render::GpuParticleEmitterMetrics metrics = renderer.GpuParticleMetrics(created.handle);
    EXPECT_EQ(metrics.programFingerprint, program.fingerprint);
    EXPECT_EQ(metrics.artifactFingerprint, artifact.artifact.artifactFingerprint);
    EXPECT_EQ(metrics.submittedSteps, 1U);
    EXPECT_EQ(metrics.submittedSpawnAttempts, cpuOracle.Reference().Counters().spawnAttempts);
    EXPECT_EQ(metrics.spawnDispatches, 1U);
    EXPECT_EQ(metrics.particleBufferCreations, 1U);
    EXPECT_EQ(metrics.particleBufferGrowths, 0U);
    EXPECT_EQ(metrics.normalFrameReadbacks, 0U);
    EXPECT_EQ(metrics.normalFrameFenceWaits, 0U);

    ASSERT_TRUE(cpuOracle.Step());
    ASSERT_TRUE(renderer.StepGpuParticleEmitter(step));
    ASSERT_EQ(cpuOracle.Reference().AliveCount(), 1U);
    ASSERT_TRUE(cpuOracle.Reference().TryGetParticle(0U, cpuParticle));
    ASSERT_EQ(cpuParticle.ageFrames, 1U);

    const std::filesystem::path movedCapturePath =
        std::filesystem::temp_directory_path() / "trace2d_gpu_particle_conformance_1.bmp";
    const render::CapturedFrame movedCapture = renderer.CaptureFrame(
        render::CaptureRequest{1U, movedCapturePath, render::CaptureImageFormat::Bmp},
        camera,
        {},
        std::span<const render::GpuParticleRenderData>{&draw, 1U});

    BrightBounds movedBounds{};
    ASSERT_TRUE(TryFindBrightBounds(movedCapture, movedBounds));
    EXPECT_NEAR(
        PixelCenterX(movedBounds),
        ExpectedPixelCenterX(movedCapture, camera, cpuParticle),
        2.0F);
    EXPECT_GT(PixelCenterX(movedBounds), PixelCenterX(firstBounds));

    std::uint32_t submittedSteps = 2U;
    while (cpuOracle.Reference().AliveCount() != 0U && submittedSteps <= sampledLifetime)
    {
        ASSERT_TRUE(cpuOracle.Step());
        ASSERT_TRUE(renderer.StepGpuParticleEmitter(step));
        ++submittedSteps;
    }
    ASSERT_EQ(cpuOracle.Reference().AliveCount(), 0U);
    EXPECT_EQ(cpuOracle.Reference().Counters().expired, 1U);

    const std::filesystem::path expiredCapturePath =
        std::filesystem::temp_directory_path() / "trace2d_gpu_particle_conformance_expired.bmp";
    const render::CapturedFrame expiredCapture = renderer.CaptureFrame(
        render::CaptureRequest{
            static_cast<std::uint64_t>(submittedSteps - 1U),
            expiredCapturePath,
            render::CaptureImageFormat::Bmp},
        camera,
        {},
        std::span<const render::GpuParticleRenderData>{&draw, 1U});

    BrightBounds expiredBounds{};
    EXPECT_FALSE(TryFindBrightBounds(expiredCapture, expiredBounds));

    metrics = renderer.GpuParticleMetrics(created.handle);
    EXPECT_EQ(metrics.submittedSteps, submittedSteps);
    EXPECT_EQ(metrics.submittedSpawnAttempts, cpuOracle.Reference().Counters().spawnAttempts);
    EXPECT_EQ(metrics.particleBufferCreations, 1U);
    EXPECT_EQ(metrics.particleBufferGrowths, 0U);
    EXPECT_EQ(metrics.normalFrameReadbacks, 0U);
    EXPECT_EQ(metrics.normalFrameFenceWaits, 0U);
    EXPECT_EQ(metrics.instanceUpperBound, 1U);
    EXPECT_GE(metrics.retainedGpuBytes, metrics.particleBufferBytes);

    renderer.DestroyGpuParticleEmitter(created.handle);
    renderer.DestroyTexture(texture);
    RemoveCapture(firstCapturePath);
    RemoveCapture(movedCapturePath);
    RemoveCapture(expiredCapturePath);
}
} // namespace
