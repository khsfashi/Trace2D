#pragma once

#include <trace2d/particles/ParticleDeterminism.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>

namespace trace2d::particles
{
struct ParticleVec2 final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const ParticleVec2&) const noexcept = default;
};

struct ParticleColor final
{
    float r{1.0F};
    float g{1.0F};
    float b{1.0F};
    float a{1.0F};

    [[nodiscard]] bool operator==(const ParticleColor&) const noexcept = default;
};

struct ParticleFloatRange final
{
    float minValue{0.0F};
    float maxValue{0.0F};

    [[nodiscard]] bool operator==(const ParticleFloatRange&) const noexcept = default;
};

struct ParticleUIntRange final
{
    std::uint32_t minValue{1};
    std::uint32_t maxValue{1};

    [[nodiscard]] bool operator==(const ParticleUIntRange&) const noexcept = default;
};

struct ParticleColorRange final
{
    ParticleColor minValue{};
    ParticleColor maxValue{};

    [[nodiscard]] bool operator==(const ParticleColorRange&) const noexcept = default;
};

enum class ParticleSimulationSpace : std::uint8_t
{
    Local = 0,
    World = 1,
};

enum class ParticleSpawnShapeType : std::uint8_t
{
    Point = 0,
    Box = 1,
    Circle = 2,
};

struct ParticleSpawnShape final
{
    ParticleSpawnShapeType type{ParticleSpawnShapeType::Point};
    ParticleVec2 offset{};
    ParticleVec2 boxHalfExtents{};
    float circleRadius{0.0F};

    [[nodiscard]] bool operator==(const ParticleSpawnShape&) const noexcept = default;
};

struct ParticleBurst final
{
    ParticleFrameIndex frame{0};
    std::uint32_t count{0};

    [[nodiscard]] bool operator==(const ParticleBurst&) const noexcept = default;
};

struct ParticleReferenceDefinition final
{
    std::uint32_t maxParticles{1};
    std::uint64_t globalSeed{0};
    ParticleEmitterStableId emitterStableId{0};

    ParticleFrameIndex periodicStartFrame{0};
    std::uint32_t periodicCount{0};
    std::uint32_t periodicEveryFrames{0};

    ParticleSpawnShape spawnShape{};
    ParticleUIntRange lifetimeFrames{1, 1};
    ParticleFloatRange speed{0.0F, 0.0F};
    ParticleFloatRange angleRadians{0.0F, 0.0F};
    ParticleVec2 acceleration{};
    ParticleFloatRange initialSize{1.0F, 1.0F};
    float endSizeMultiplier{1.0F};
    ParticleFloatRange rotationRadians{0.0F, 0.0F};
    ParticleFloatRange angularVelocityRadiansPerFrame{0.0F, 0.0F};
    ParticleColorRange initialColor{};
    ParticleColor endColor{};
    std::uint32_t spriteChoiceCount{1};
    ParticleSimulationSpace simulationSpace{ParticleSimulationSpace::Local};

    [[nodiscard]] bool operator==(const ParticleReferenceDefinition&) const noexcept = default;
};

struct ParticleReferenceLimits final
{
    std::uint32_t maxParticlesPerEmitter{65'536};
    std::uint32_t maxBursts{4'096};
    std::uint32_t maxSpawnAttemptsPerFrame{65'536};
};

enum class ParticleReferenceError : std::uint8_t
{
    None = 0,
    ZeroCapacity,
    CapacityExceedsLimit,
    TooManyBursts,
    BurstsNotOrdered,
    SpawnAttemptsPerFrameExceedLimit,
    InvalidPeriodicEmission,
    InvalidSpawnShape,
    InvalidLifetimeRange,
    InvalidFloatRange,
    InvalidColorRange,
    InvalidSpriteChoiceCount,
    AllocationFailed,
};

struct ParticleReferencePrepareResult final
{
    ParticleReferenceError error{ParticleReferenceError::None};

    [[nodiscard]] bool Ok() const noexcept
    {
        return error == ParticleReferenceError::None;
    }
};

struct ParticleReferenceParticle final
{
    ParticleSpawnOrdinal spawnOrdinal{0};
    ParticleVec2 position{};
    ParticleVec2 velocity{};
    ParticleVec2 acceleration{};
    std::uint32_t ageFrames{0};
    std::uint32_t lifetimeFrames{0};
    float initialSize{1.0F};
    float size{1.0F};
    float rotationRadians{0.0F};
    float angularVelocityRadiansPerFrame{0.0F};
    ParticleColor initialColor{};
    ParticleColor color{};
    std::uint32_t spriteIndex{0};
    ParticleSimulationSpace simulationSpace{ParticleSimulationSpace::Local};

    [[nodiscard]] bool operator==(const ParticleReferenceParticle&) const noexcept = default;
};

struct ParticleReferenceCounters final
{
    std::uint64_t spawnAttempts{0};
    std::uint64_t spawned{0};
    std::uint64_t updated{0};
    std::uint64_t expired{0};
    std::uint64_t dropped{0};
    std::uint32_t peakAlive{0};

    [[nodiscard]] bool operator==(const ParticleReferenceCounters&) const noexcept = default;
};

struct ParticleReferenceMemoryReport final
{
    std::uint32_t capacity{0};
    std::size_t bytesPerParticlePayload{0};
    std::size_t particleStorageBytes{0};
    std::size_t burstScheduleBytes{0};
    std::size_t preparedPayloadBytes{0};
    std::uint32_t storageBlockCount{0};
    std::uint32_t steadyStateSimulationAllocations{0};

    [[nodiscard]] bool operator==(const ParticleReferenceMemoryReport&) const noexcept = default;
};

class ParticleReferenceEmitter final
{
public:
    ParticleReferenceEmitter() noexcept = default;
    ~ParticleReferenceEmitter() = default;

    ParticleReferenceEmitter(const ParticleReferenceEmitter&) = delete;
    ParticleReferenceEmitter& operator=(const ParticleReferenceEmitter&) = delete;
    ParticleReferenceEmitter(ParticleReferenceEmitter&&) noexcept = default;
    ParticleReferenceEmitter& operator=(ParticleReferenceEmitter&&) noexcept = default;

    [[nodiscard]] ParticleReferencePrepareResult Prepare(
        const ParticleReferenceDefinition& definition,
        std::span<const ParticleBurst> bursts,
        const ParticleReferenceLimits& limits = {}) noexcept;

    void Reset() noexcept;
    [[nodiscard]] bool Step() noexcept;

    [[nodiscard]] bool IsPrepared() const noexcept;
    [[nodiscard]] const ParticleReferenceDefinition& Definition() const noexcept;
    [[nodiscard]] ParticleFrameIndex NextFrameIndex() const noexcept;
    [[nodiscard]] std::uint32_t AliveCount() const noexcept;
    [[nodiscard]] ParticleSpawnOrdinal NextSpawnOrdinal() const noexcept;
    [[nodiscard]] const ParticleReferenceCounters& Counters() const noexcept;
    [[nodiscard]] ParticleReferenceMemoryReport MemoryReport() const noexcept;

    [[nodiscard]] bool TryGetParticle(
        std::uint32_t aliveIndex,
        ParticleReferenceParticle& particle) const noexcept;
    [[nodiscard]] bool TryGetParticleBySpawnOrdinal(
        ParticleSpawnOrdinal spawnOrdinal,
        ParticleReferenceParticle& particle) const noexcept;

    [[nodiscard]] static constexpr std::size_t BytesPerParticlePayload() noexcept
    {
        return sizeof(ParticleSpawnOrdinal) +
            (3U * sizeof(ParticleVec2)) +
            (2U * sizeof(std::uint32_t)) +
            (4U * sizeof(float)) +
            (2U * sizeof(ParticleColor)) +
            sizeof(std::uint32_t);
    }

private:
    struct Storage final
    {
        std::unique_ptr<ParticleSpawnOrdinal[]> spawnOrdinals{};
        std::unique_ptr<ParticleVec2[]> positions{};
        std::unique_ptr<ParticleVec2[]> velocities{};
        std::unique_ptr<ParticleVec2[]> accelerations{};
        std::unique_ptr<std::uint32_t[]> ageFrames{};
        std::unique_ptr<std::uint32_t[]> lifetimeFrames{};
        std::unique_ptr<float[]> initialSizes{};
        std::unique_ptr<float[]> sizes{};
        std::unique_ptr<float[]> rotations{};
        std::unique_ptr<float[]> angularVelocities{};
        std::unique_ptr<ParticleColor[]> initialColors{};
        std::unique_ptr<ParticleColor[]> colors{};
        std::unique_ptr<std::uint32_t[]> spriteIndices{};
    };

    [[nodiscard]] ParticleRandomKey MakeRandomKey(
        ParticleSpawnOrdinal ordinal,
        ParticleRandomChannel channel) const noexcept;
    [[nodiscard]] float SampleFloat(
        ParticleSpawnOrdinal ordinal,
        ParticleRandomChannel channel,
        const ParticleFloatRange& range) const noexcept;
    [[nodiscard]] std::uint32_t SampleUIntInclusive(
        ParticleSpawnOrdinal ordinal,
        ParticleRandomChannel channel,
        const ParticleUIntRange& range) const noexcept;
    [[nodiscard]] ParticleColor SampleInitialColor(ParticleSpawnOrdinal ordinal) const noexcept;
    [[nodiscard]] ParticleVec2 SampleSpawnPosition(ParticleSpawnOrdinal ordinal) const noexcept;
    [[nodiscard]] ParticleVec2 SampleInitialVelocity(ParticleSpawnOrdinal ordinal) const noexcept;

    void UpdateExisting() noexcept;
    void EmitCurrentFrame() noexcept;
    void AttemptSpawn() noexcept;
    void ApplyOverLife(std::uint32_t index) noexcept;
    void CopyParticle(std::uint32_t fromIndex, std::uint32_t toIndex) noexcept;
    [[nodiscard]] ParticleReferenceParticle ReadParticle(std::uint32_t aliveIndex) const noexcept;

    ParticleReferenceDefinition definition_{};
    ParticleReferenceLimits limits_{};
    Storage storage_{};
    std::unique_ptr<ParticleBurst[]> bursts_{};
    std::uint32_t burstCount_{0};
    std::uint32_t nextBurstIndex_{0};
    std::uint32_t aliveCount_{0};
    ParticleFrameIndex nextFrameIndex_{0};
    ParticleSpawnOrdinal nextSpawnOrdinal_{0};
    ParticleReferenceCounters counters_{};
    bool prepared_{false};
};

static_assert(std::is_trivially_copyable_v<ParticleVec2>);
static_assert(std::is_trivially_copyable_v<ParticleColor>);
static_assert(std::is_trivially_copyable_v<ParticleReferenceParticle>);
static_assert(ParticleReferenceEmitter::BytesPerParticlePayload() == 92U);
} // namespace trace2d::particles
