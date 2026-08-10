#pragma once

#include <trace2d/particles/ParticleEffect.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::particles
{
using ParticleProgramMask = std::uint64_t;

enum class ParticleProgramFeature : std::uint8_t
{
    PeriodicEmission = 0,
    Bursts,
    SpawnPoint,
    SpawnBox,
    SpawnCircle,
    VariableLifetime,
    InitialMotion,
    Acceleration,
    VariableInitialSize,
    SizeOverLife,
    InitialRotation,
    AngularVelocity,
    VariableInitialColor,
    ColorOverLife,
    SpriteChoice,
    WorldSpace,
    Looping,
    AdditiveBlend,
};

enum class ParticleProgramAttribute : std::uint8_t
{
    SpawnOrdinal = 0,
    Position,
    Velocity,
    Acceleration,
    AgeFrames,
    LifetimeFrames,
    InitialSize,
    Size,
    Rotation,
    AngularVelocity,
    InitialColor,
    Color,
    SpriteIndex,
};

enum class ParticleProgramOperation : std::uint8_t
{
    SpawnPositionRandom = 0,
    SpawnLifetimeRandom,
    SpawnSpeedRandom,
    SpawnAngleRandom,
    SpawnSizeRandom,
    SpawnRotationRandom,
    SpawnAngularVelocityRandom,
    SpawnColorRandom,
    SpawnSpriteRandom,
    ApplyAcceleration,
    IntegratePosition,
    IntegrateRotation,
    AdvanceLifetime,
    EvaluateSizeOverLife,
    EvaluateColorOverLife,
    Count,
};

inline constexpr std::size_t ParticleProgramOperationCount =
    static_cast<std::size_t>(ParticleProgramOperation::Count);
inline constexpr std::size_t ParticleProgramRandomChannelCapacity = 13U;
inline constexpr std::size_t ParticleGpuRuntimeFieldCapacity = 9U;

[[nodiscard]] constexpr ParticleProgramMask ParticleProgramBit(
    const ParticleProgramFeature feature) noexcept
{
    return ParticleProgramMask{1} << static_cast<std::uint8_t>(feature);
}

[[nodiscard]] constexpr ParticleProgramMask ParticleProgramBit(
    const ParticleProgramAttribute attribute) noexcept
{
    return ParticleProgramMask{1} << static_cast<std::uint8_t>(attribute);
}

[[nodiscard]] constexpr bool HasParticleProgramFeature(
    const ParticleProgramMask mask,
    const ParticleProgramFeature feature) noexcept
{
    return (mask & ParticleProgramBit(feature)) != 0U;
}

[[nodiscard]] constexpr bool HasParticleProgramAttribute(
    const ParticleProgramMask mask,
    const ParticleProgramAttribute attribute) noexcept
{
    return (mask & ParticleProgramBit(attribute)) != 0U;
}

struct ParticleProgramOperationCost final
{
    ParticleProgramOperation operation{ParticleProgramOperation::SpawnPositionRandom};
    std::uint32_t perAdmittedSpawn{0};
    std::uint32_t perUpdatedParticle{0};
    std::uint32_t perSurvivingUpdatedParticle{0};

    [[nodiscard]] bool operator==(const ParticleProgramOperationCost&) const noexcept = default;
};

enum class ParticleGpuRuntimeFieldKind : std::uint8_t
{
    Position = 0,
    Velocity,
    AgeFrames,
    LifetimeFrames,
    InitialSize,
    InitialRotation,
    AngularVelocity,
    InitialColor,
    SpriteIndex,
};

struct ParticleGpuRuntimeField final
{
    ParticleGpuRuntimeFieldKind kind{ParticleGpuRuntimeFieldKind::AgeFrames};
    std::uint32_t offsetBytes{0};
    std::uint32_t sizeBytes{0};

    [[nodiscard]] bool operator==(const ParticleGpuRuntimeField&) const noexcept = default;
};

struct ParticleProgram final
{
    std::string effectAssetId{};
    std::string semanticId{};
    ParticleEffectBackend selectedBackend{ParticleEffectBackend::Cpu};
    ParticleEffectLifecycle lifecycle{};
    ParticleReferenceDefinition definition{};
    std::vector<ParticleBurst> bursts{};
    std::vector<std::string> spriteReferences{};
    ParticleBlendMode blendMode{ParticleBlendMode::Alpha};

    std::uint64_t fingerprint{0};
    ParticleProgramMask featureMask{0};
    ParticleProgramMask spawnAttributeMask{0};
    ParticleProgramMask updateReadAttributeMask{0};
    ParticleProgramMask updateWriteAttributeMask{0};
    ParticleProgramMask renderOnlyAttributeMask{0};
    ParticleProgramMask cpuStoredAttributeMask{0};
    ParticleProgramMask constantAttributeMask{0};
    ParticleProgramMask derivedGpuAttributeMask{0};

    std::array<ParticleRandomChannel, ParticleProgramRandomChannelCapacity> requiredRandomChannels{};
    std::uint32_t requiredRandomChannelCount{0};
    std::array<ParticleProgramOperationCost, ParticleProgramOperationCount> operationCosts{};

    std::array<ParticleGpuRuntimeField, ParticleGpuRuntimeFieldCapacity> gpuFields{};
    std::uint32_t gpuFieldCount{0};
    std::uint32_t gpuStrideBytes{0};
    std::uint64_t gpuBufferBytes{0};
    std::uint64_t gpuPipelineVariantId{0};
};

[[nodiscard]] ParticleProgram CompileParticleProgram(const ParticleEffectAsset& effect);

enum class ParticleGpuCompileError : std::uint8_t
{
    None = 0,
    BackendNotSelected,
    UnsupportedFeature,
};

struct ParticleGpuCompileArtifact final
{
    std::uint64_t programFingerprint{0};
    std::uint64_t artifactFingerprint{0};
    std::uint64_t pipelineVariantId{0};
    std::uint32_t capacity{0};
    std::array<ParticleGpuRuntimeField, ParticleGpuRuntimeFieldCapacity> fields{};
    std::uint32_t fieldCount{0};
    std::uint32_t strideBytes{0};
    std::uint64_t bufferBytes{0};

    [[nodiscard]] bool operator==(const ParticleGpuCompileArtifact&) const noexcept = default;
};

struct ParticleGpuCompileResult final
{
    ParticleGpuCompileError error{ParticleGpuCompileError::None};
    ParticleGpuCompileArtifact artifact{};
    std::string message{};

    [[nodiscard]] bool Ok() const noexcept
    {
        return error == ParticleGpuCompileError::None;
    }
};

[[nodiscard]] ParticleGpuCompileResult CompileParticleGpuArtifact(const ParticleProgram& program);

struct ParticleCostObservation final
{
    std::uint32_t emitterCount{1};
    std::uint64_t observedFrames{0};
    std::uint32_t currentAlive{0};
    std::uint32_t peakAlive{0};
    ParticleReferenceCounters counters{};
    ParticleReferenceMemoryReport memory{};
};

struct ParticleOperationTotal final
{
    ParticleProgramOperation operation{ParticleProgramOperation::SpawnPositionRandom};
    std::uint64_t evaluations{0};

    [[nodiscard]] bool operator==(const ParticleOperationTotal&) const noexcept = default;
};

struct ParticleStructuralCostReport final
{
    std::uint64_t programFingerprint{0};
    ParticleEffectBackend selectedBackend{ParticleEffectBackend::Cpu};
    std::uint32_t capacityPerEmitter{0};
    std::uint32_t emitterCount{0};
    std::uint64_t observedFrames{0};
    std::uint32_t currentAlive{0};
    std::uint32_t peakAlive{0};
    ParticleReferenceCounters counters{};

    ParticleProgramMask cpuStoredAttributeMask{0};
    std::size_t bytesPerParticlePayload{0};
    std::size_t particleStorageBytes{0};
    std::size_t preparedCpuStateBytes{0};
    std::uint32_t steadyStateSimulationAllocations{0};

    std::uint64_t spawnRandomEvaluations{0};
    std::uint64_t particleUpdates{0};
    std::uint64_t survivingParticleUpdates{0};
    std::array<ParticleOperationTotal, ParticleProgramOperationCount> operationTotals{};

    std::uint32_t plannedGpuStrideBytes{0};
    std::uint64_t plannedGpuBufferBytesPerEmitter{0};

    [[nodiscard]] bool operator==(const ParticleStructuralCostReport&) const noexcept = default;
};

[[nodiscard]] ParticleStructuralCostReport BuildParticleStructuralCostReport(
    const ParticleProgram& program,
    const ParticleCostObservation& observation) noexcept;

class ParticleCostAccumulator final
{
public:
    void Reset(const ParticleEmitter2D& emitter) noexcept;
    void ObserveAfterStep(const ParticleEmitter2D& emitter) noexcept;

    [[nodiscard]] ParticleCostObservation Observation(
        const ParticleEmitter2D& emitter,
        std::uint32_t emitterCount = 1U) const noexcept;

private:
    ParticleReferenceCounters previous_{};
    ParticleReferenceCounters totals_{};
    std::uint64_t observedFrames_{0};
    std::uint32_t peakAlive_{0};
    bool initialized_{false};
};

[[nodiscard]] ParticleEmitter2DPrepareResult PrepareParticleProgramCpuEmitter(
    const ParticleProgram& program,
    std::uint64_t globalSeed,
    ParticleEmitterStableId stableId,
    ParticleEmitter2D& emitter,
    const ParticleReferenceLimits& limits = {});

[[nodiscard]] std::string_view ToString(ParticleEffectBackend backend) noexcept;
[[nodiscard]] std::string_view ToString(ParticleProgramFeature feature) noexcept;
[[nodiscard]] std::string_view ToString(ParticleProgramAttribute attribute) noexcept;
[[nodiscard]] std::string_view ToString(ParticleProgramOperation operation) noexcept;
[[nodiscard]] std::string_view ToString(ParticleGpuRuntimeFieldKind field) noexcept;
[[nodiscard]] std::string_view ToString(ParticleRandomChannel channel) noexcept;
[[nodiscard]] std::string_view ToString(ParticleGpuCompileError error) noexcept;
} // namespace trace2d::particles
