#pragma once

#include <trace2d/particles/ParticleDeterminism.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::particles
{
class ParticleEmitter2D;
}

namespace trace2d::agent
{
inline constexpr std::uint32_t MaxParticleInspectionCount = 1'024U;

enum class ParticleInspectionErrorCode : std::uint8_t
{
    EmitterUnavailable = 0,
    EmitterNotPrepared,
    InvalidRange,
    ParticleNotFound,
    InvalidAssertion,
    TypeMismatch,
    StateMismatch,
};

[[nodiscard]] std::string_view ToString(ParticleInspectionErrorCode code) noexcept;

enum class ParticleValueKind : std::uint8_t
{
    Boolean = 0,
    UnsignedInteger,
    Float,
    String,
};

[[nodiscard]] std::string_view ToString(ParticleValueKind kind) noexcept;

struct ParticleValue final
{
    ParticleValueKind kind{ParticleValueKind::UnsignedInteger};
    bool booleanValue{false};
    std::uint64_t unsignedIntegerValue{0};
    float floatValue{0.0F};
    std::string stringValue{};

    [[nodiscard]] static ParticleValue Boolean(bool value) noexcept;
    [[nodiscard]] static ParticleValue Unsigned(std::uint64_t value) noexcept;
    [[nodiscard]] static ParticleValue Float(float value) noexcept;
    [[nodiscard]] static ParticleValue String(std::string value) noexcept;

    [[nodiscard]] bool operator==(const ParticleValue&) const noexcept = default;
};

enum class ParticleSimulationSpaceSnapshot : std::uint8_t
{
    Local = 0,
    World,
};

[[nodiscard]] std::string_view ToString(ParticleSimulationSpaceSnapshot space) noexcept;

struct ParticleVector2Snapshot final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const ParticleVector2Snapshot&) const noexcept = default;
};

struct ParticleColorSnapshot final
{
    float r{1.0F};
    float g{1.0F};
    float b{1.0F};
    float a{1.0F};

    [[nodiscard]] bool operator==(const ParticleColorSnapshot&) const noexcept = default;
};

struct ParticleStateSnapshot final
{
    particles::ParticleSpawnOrdinal spawnOrdinal{0};
    ParticleVector2Snapshot position{};
    ParticleVector2Snapshot velocity{};
    ParticleVector2Snapshot acceleration{};
    std::uint32_t ageFrames{0};
    std::uint32_t lifetimeFrames{0};
    float initialSize{1.0F};
    float size{1.0F};
    float rotationRadians{0.0F};
    float angularVelocityRadiansPerFrame{0.0F};
    ParticleColorSnapshot initialColor{};
    ParticleColorSnapshot color{};
    std::uint32_t spriteIndex{0};
    ParticleSimulationSpaceSnapshot simulationSpace{ParticleSimulationSpaceSnapshot::Local};

    [[nodiscard]] bool operator==(const ParticleStateSnapshot&) const noexcept = default;
};

struct ParticleEmitterBinding final
{
    std::string_view entitySemanticId{};
    const particles::ParticleEmitter2D* emitter{nullptr};
};

struct ParticleInspectionError final
{
    ParticleInspectionErrorCode code{ParticleInspectionErrorCode::EmitterUnavailable};
    std::string message{};

    [[nodiscard]] bool operator==(const ParticleInspectionError&) const noexcept = default;
};

struct ParticleEmitterSnapshot final
{
    std::string entitySemanticId{};
    std::string effectSemanticId{};
    std::string effectAssetId{};
    particles::ParticleEmitterStableId emitterStableId{0};
    bool prepared{false};
    bool playing{false};
    std::uint32_t cycleFrame{0};
    std::uint64_t completedLoops{0};
    particles::ParticleFrameIndex nextSimulationFrame{0};
    std::uint32_t aliveCount{0};
    std::uint32_t capacity{0};
    std::uint64_t spawnAttemptsTotal{0};
    std::uint64_t emittedTotal{0};
    std::uint64_t updatedTotal{0};
    std::uint64_t expiredTotal{0};
    std::uint64_t droppedTotal{0};
    std::uint32_t peakAlive{0};
    std::uint64_t stateFingerprint{0};

    [[nodiscard]] bool operator==(const ParticleEmitterSnapshot&) const noexcept = default;
};

struct ParticleEmitterInspectionResult final
{
    std::optional<ParticleEmitterSnapshot> snapshot{};
    std::optional<ParticleInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

struct ParticleDetailSnapshot final
{
    std::string entitySemanticId{};
    std::string effectSemanticId{};
    particles::ParticleEmitterStableId emitterStableId{0};
    std::uint32_t totalAlive{0};
    std::uint32_t offset{0};
    std::uint32_t requestedLimit{0};
    std::vector<ParticleStateSnapshot> particles{};

    [[nodiscard]] bool operator==(const ParticleDetailSnapshot&) const noexcept = default;
};

struct ParticleDetailInspectionResult final
{
    std::optional<ParticleDetailSnapshot> snapshot{};
    std::optional<ParticleInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

struct ParticleSingleInspectionResult final
{
    std::optional<ParticleStateSnapshot> particle{};
    std::optional<ParticleInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return particle.has_value() && !error.has_value();
    }
};

enum class ParticleAssertionField : std::uint8_t
{
    Prepared = 0,
    Playing,
    CycleFrame,
    CompletedLoops,
    NextSimulationFrame,
    AliveCount,
    Capacity,
    SpawnAttemptsTotal,
    EmittedTotal,
    UpdatedTotal,
    ExpiredTotal,
    DroppedTotal,
    PeakAlive,
    StateFingerprint,
    SpawnOrdinal,
    PositionX,
    PositionY,
    VelocityX,
    VelocityY,
    AccelerationX,
    AccelerationY,
    AgeFrames,
    LifetimeFrames,
    InitialSize,
    Size,
    RotationRadians,
    AngularVelocityRadiansPerFrame,
    InitialColorR,
    InitialColorG,
    InitialColorB,
    InitialColorA,
    ColorR,
    ColorG,
    ColorB,
    ColorA,
    SpriteIndex,
    SimulationSpace,
};

[[nodiscard]] std::string_view ToString(ParticleAssertionField field) noexcept;

struct ParticleAssertion final
{
    ParticleAssertionField field{ParticleAssertionField::AliveCount};
    std::optional<particles::ParticleSpawnOrdinal> spawnOrdinal{};
    ParticleValue expected{};

    [[nodiscard]] bool operator==(const ParticleAssertion&) const noexcept = default;
};

struct ParticleAssertionContext final
{
    std::string entitySemanticId{};
    std::string effectSemanticId{};
    std::string effectAssetId{};
    particles::ParticleEmitterStableId emitterStableId{0};
    particles::ParticleFrameIndex nextSimulationFrame{0};
    std::uint32_t cycleFrame{0};
    std::uint64_t seed{0};
    std::uint32_t aliveCount{0};
    std::optional<ParticleStateSnapshot> particleDetail{};

    [[nodiscard]] bool operator==(const ParticleAssertionContext&) const noexcept = default;
};

struct ParticleAssertionResult final
{
    ParticleAssertion assertion{};
    std::optional<ParticleValue> observed{};
    ParticleAssertionContext context{};
    std::optional<ParticleInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return observed.has_value() && !error.has_value();
    }
};
} // namespace trace2d::agent
