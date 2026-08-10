#pragma once

#include <trace2d/render/RenderData.hpp>

#include <cstdint>

namespace trace2d::particles
{
struct ParticleProgram;
}

namespace trace2d::render
{
using GpuParticleEmitterHandle = std::uint32_t;
inline constexpr GpuParticleEmitterHandle InvalidGpuParticleEmitterHandle = 0U;

enum class GpuParticleRuntimeError : std::uint8_t
{
    None = 0,
    BackendNotSelected,
    UnsupportedFeature,
    InvalidArtifact,
    InvalidTexture,
    InvalidHandle,
};

struct GpuParticleRuntimeSupport final
{
    GpuParticleRuntimeError error{GpuParticleRuntimeError::None};
    std::uint64_t unsupportedFeatureMask{0};
    std::uint64_t programFingerprint{0};
    std::uint64_t artifactFingerprint{0};
    std::uint64_t pipelineVariantId{0};
    std::uint32_t capacity{0};
    std::uint32_t fieldCount{0};
    std::uint32_t strideBytes{0};
    std::uint64_t particleBufferBytes{0};

    [[nodiscard]] bool Ok() const noexcept
    {
        return error == GpuParticleRuntimeError::None;
    }
};

[[nodiscard]] GpuParticleRuntimeSupport AnalyzeGpuParticleRuntimeSupport(
    const particles::ParticleProgram& program);

struct GpuParticleEmitterCreateResult final
{
    GpuParticleEmitterHandle handle{InvalidGpuParticleEmitterHandle};
    GpuParticleRuntimeSupport support{};

    [[nodiscard]] bool Ok() const noexcept
    {
        return handle != InvalidGpuParticleEmitterHandle && support.Ok();
    }
};

struct GpuParticleStepData final
{
    GpuParticleEmitterHandle emitter{InvalidGpuParticleEmitterHandle};
    Float2 emitterWorldPosition{};
};

struct GpuParticleRenderData final
{
    GpuParticleEmitterHandle emitter{InvalidGpuParticleEmitterHandle};
    Float2 emitterWorldPosition{};
    std::int32_t layer{0};
    std::uint64_t stableOrder{0};
};

struct GpuParticleEmitterMetrics final
{
    std::uint64_t programFingerprint{0};
    std::uint64_t artifactFingerprint{0};
    std::uint64_t pipelineVariantId{0};
    std::uint32_t capacity{0};
    std::uint32_t strideBytes{0};
    std::uint64_t particleBufferBytes{0};
    std::uint64_t retainedGpuBytes{0};
    std::uint64_t submittedSteps{0};
    std::uint64_t submittedSpawnAttempts{0};
    std::uint64_t updateDispatches{0};
    std::uint64_t spawnDispatches{0};
    std::uint64_t clearDispatches{0};
    std::uint64_t renderDraws{0};
    std::uint64_t renderInstances{0};
    std::uint32_t instanceUpperBound{0};
    std::uint32_t particleBufferCreations{0};
    std::uint32_t particleBufferGrowths{0};
    std::uint64_t normalFrameReadbacks{0};
    std::uint64_t normalFrameFenceWaits{0};
};
} // namespace trace2d::render
