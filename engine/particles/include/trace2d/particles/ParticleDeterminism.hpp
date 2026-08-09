#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace trace2d::particles
{
using ParticleFrameIndex = std::uint64_t;
using ParticleEmitterStableId = std::uint64_t;
using ParticleSpawnOrdinal = std::uint64_t;

enum class ParticleFramePhase : std::uint8_t
{
    ApplyCommands = 0,
    UpdateExisting = 1,
    ExpireExisting = 2,
    Emit = 3,
    Observe = 4,
    ExtractBackend = 5,
};

inline constexpr std::size_t ParticleFramePhaseCount = 6;

struct ParticleLifetimeTransition final
{
    std::uint32_t ageAfterUpdate{0};
    bool expiresBeforeObservation{false};

    [[nodiscard]] bool operator==(const ParticleLifetimeTransition&) const noexcept = default;
};

enum class ParticleRandomChannel : std::uint32_t
{
    SpawnPositionX = 0x00010001U,
    SpawnPositionY = 0x00010002U,
    Lifetime = 0x00020001U,
    Speed = 0x00030001U,
    Angle = 0x00030002U,
    Rotation = 0x00040001U,
    AngularVelocity = 0x00040002U,
    Size = 0x00050001U,
    ColorR = 0x00060001U,
    ColorG = 0x00060002U,
    ColorB = 0x00060003U,
    ColorA = 0x00060004U,
    SpriteChoice = 0x00070001U,
};

struct ParticleRandomKey final
{
    std::uint64_t globalSeed{0};
    ParticleEmitterStableId emitterStableId{0};
    ParticleSpawnOrdinal spawnOrdinal{0};
    ParticleRandomChannel channel{ParticleRandomChannel::SpawnPositionX};

    [[nodiscard]] bool operator==(const ParticleRandomKey&) const noexcept = default;
};

[[nodiscard]] std::span<const ParticleFramePhase> ParticleFrameOrder() noexcept;

[[nodiscard]] bool IsParticleObservable(
    std::uint32_t ageFrames,
    std::uint32_t lifetimeFrames) noexcept;

[[nodiscard]] ParticleLifetimeTransition AdvanceExistingParticleLifetime(
    std::uint32_t ageFrames,
    std::uint32_t lifetimeFrames) noexcept;

[[nodiscard]] std::uint64_t ParticleRandomBits(const ParticleRandomKey& key) noexcept;
[[nodiscard]] std::uint32_t ParticleRandomU32(const ParticleRandomKey& key) noexcept;
[[nodiscard]] float ParticleRandomUnitFloat(const ParticleRandomKey& key) noexcept;
[[nodiscard]] float ParticleRandomFloatRange(
    const ParticleRandomKey& key,
    float minInclusive,
    float maxExclusive) noexcept;

static_assert(std::is_trivially_copyable_v<ParticleLifetimeTransition>);
static_assert(std::is_trivially_copyable_v<ParticleRandomKey>);
} // namespace trace2d::particles
