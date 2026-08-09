#include <trace2d/particles/ParticleDeterminism.hpp>

#include <array>

namespace trace2d::particles
{
namespace
{
constexpr std::array<ParticleFramePhase, ParticleFramePhaseCount> FrameOrder{
    ParticleFramePhase::ApplyCommands,
    ParticleFramePhase::UpdateExisting,
    ParticleFramePhase::ExpireExisting,
    ParticleFramePhase::Emit,
    ParticleFramePhase::Observe,
    ParticleFramePhase::ExtractBackend,
};

constexpr std::uint64_t SeedDomain = 0x243F6A8885A308D3ULL;
constexpr std::uint64_t EmitterDomain = 0x13198A2E03707344ULL;
constexpr std::uint64_t OrdinalDomain = 0xA4093822299F31D0ULL;
constexpr std::uint64_t ChannelDomain = 0x082EFA98EC4E6C89ULL;
constexpr float UnitFloatScale = 1.0F / 16777216.0F;

[[nodiscard]] constexpr std::uint64_t Mix64(std::uint64_t value) noexcept
{
    value ^= value >> 30U;
    value *= 0xBF58476D1CE4E5B9ULL;
    value ^= value >> 27U;
    value *= 0x94D049BB133111EBULL;
    value ^= value >> 31U;
    return value;
}
} // namespace

std::span<const ParticleFramePhase> ParticleFrameOrder() noexcept
{
    return FrameOrder;
}

bool IsParticleObservable(
    const std::uint32_t ageFrames,
    const std::uint32_t lifetimeFrames) noexcept
{
    return lifetimeFrames != 0U && ageFrames < lifetimeFrames;
}

ParticleLifetimeTransition AdvanceExistingParticleLifetime(
    const std::uint32_t ageFrames,
    const std::uint32_t lifetimeFrames) noexcept
{
    if (lifetimeFrames == 0U || ageFrames >= lifetimeFrames)
    {
        return ParticleLifetimeTransition{ageFrames, true};
    }

    const std::uint32_t ageAfterUpdate = ageFrames + 1U;
    return ParticleLifetimeTransition{
        ageAfterUpdate,
        ageAfterUpdate >= lifetimeFrames,
    };
}

std::uint64_t ParticleRandomBits(const ParticleRandomKey& key) noexcept
{
    const std::uint64_t seedPart = Mix64(key.globalSeed ^ SeedDomain);
    const std::uint64_t emitterPart = Mix64(key.emitterStableId ^ EmitterDomain);
    const std::uint64_t ordinalPart = Mix64(key.spawnOrdinal ^ OrdinalDomain);
    const std::uint64_t channelPart =
        Mix64(static_cast<std::uint64_t>(key.channel) ^ ChannelDomain);

    return Mix64(seedPart ^ emitterPart ^ ordinalPart ^ channelPart);
}

std::uint32_t ParticleRandomU32(const ParticleRandomKey& key) noexcept
{
    return static_cast<std::uint32_t>(ParticleRandomBits(key) >> 32U);
}

float ParticleRandomUnitFloat(const ParticleRandomKey& key) noexcept
{
    const std::uint32_t upper24 = static_cast<std::uint32_t>(ParticleRandomBits(key) >> 40U);
    return static_cast<float>(upper24) * UnitFloatScale;
}

float ParticleRandomFloatRange(
    const ParticleRandomKey& key,
    const float minInclusive,
    const float maxExclusive) noexcept
{
    return minInclusive + ((maxExclusive - minInclusive) * ParticleRandomUnitFloat(key));
}
} // namespace trace2d::particles
