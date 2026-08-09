#include <trace2d/particles/ParticleDeterminism.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstdint>

namespace
{
using trace2d::particles::ParticleRandomChannel;
using trace2d::particles::ParticleRandomKey;

constexpr std::uint64_t TestSeed = 0x0123456789ABCDEFULL;
constexpr trace2d::particles::ParticleEmitterStableId TestEmitter = 0xFEDCBA9876543210ULL;

[[nodiscard]] ParticleRandomKey MakeKey(
    const trace2d::particles::ParticleSpawnOrdinal ordinal,
    const ParticleRandomChannel channel,
    const trace2d::particles::ParticleEmitterStableId emitter = TestEmitter,
    const std::uint64_t seed = TestSeed) noexcept
{
    return ParticleRandomKey{seed, emitter, ordinal, channel};
}

[[nodiscard]] std::array<std::uint64_t, 4> BuildResetSequence(const std::uint64_t seed) noexcept
{
    return std::array<std::uint64_t, 4>{
        trace2d::particles::ParticleRandomBits(
            MakeKey(0, ParticleRandomChannel::SpawnPositionX, TestEmitter, seed)),
        trace2d::particles::ParticleRandomBits(
            MakeKey(0, ParticleRandomChannel::Lifetime, TestEmitter, seed)),
        trace2d::particles::ParticleRandomBits(
            MakeKey(1, ParticleRandomChannel::SpawnPositionX, TestEmitter, seed)),
        trace2d::particles::ParticleRandomBits(
            MakeKey(1, ParticleRandomChannel::ColorA, TestEmitter, seed)),
    };
}

TEST(ParticleDeterminismTests, FrameOrderIsExactAndBackendExtractionIsLast)
{
    constexpr std::array<trace2d::particles::ParticleFramePhase, 6> Expected{
        trace2d::particles::ParticleFramePhase::ApplyCommands,
        trace2d::particles::ParticleFramePhase::UpdateExisting,
        trace2d::particles::ParticleFramePhase::ExpireExisting,
        trace2d::particles::ParticleFramePhase::Emit,
        trace2d::particles::ParticleFramePhase::Observe,
        trace2d::particles::ParticleFramePhase::ExtractBackend,
    };

    const std::span<const trace2d::particles::ParticleFramePhase> actual =
        trace2d::particles::ParticleFrameOrder();

    ASSERT_EQ(actual.size(), Expected.size());
    for (std::size_t index = 0; index < Expected.size(); ++index)
    {
        EXPECT_EQ(actual[index], Expected[index]);
    }
}

TEST(ParticleDeterminismTests, BurstAtFrameNIsObservableAtAgeZeroAndDoesNotUpdateImmediately)
{
    constexpr trace2d::particles::ParticleFrameIndex BurstFrame = 37;
    constexpr std::uint32_t LifetimeFrames = 2;

    const trace2d::particles::ParticleFrameIndex spawnFrame = BurstFrame;
    std::uint32_t ageFrames = 0;

    EXPECT_EQ(spawnFrame, BurstFrame);
    EXPECT_TRUE(trace2d::particles::IsParticleObservable(ageFrames, LifetimeFrames));

    const trace2d::particles::ParticleLifetimeTransition frameNPlusOne =
        trace2d::particles::AdvanceExistingParticleLifetime(ageFrames, LifetimeFrames);
    EXPECT_EQ(frameNPlusOne.ageAfterUpdate, 1U);
    EXPECT_FALSE(frameNPlusOne.expiresBeforeObservation);
    ageFrames = frameNPlusOne.ageAfterUpdate;
    EXPECT_TRUE(trace2d::particles::IsParticleObservable(ageFrames, LifetimeFrames));

    const trace2d::particles::ParticleLifetimeTransition frameNPlusTwo =
        trace2d::particles::AdvanceExistingParticleLifetime(ageFrames, LifetimeFrames);
    EXPECT_EQ(frameNPlusTwo.ageAfterUpdate, 2U);
    EXPECT_TRUE(frameNPlusTwo.expiresBeforeObservation);
    EXPECT_FALSE(trace2d::particles::IsParticleObservable(
        frameNPlusTwo.ageAfterUpdate,
        LifetimeFrames));
}

TEST(ParticleDeterminismTests, OneFrameLifetimeExpiresBeforeNextObservation)
{
    EXPECT_TRUE(trace2d::particles::IsParticleObservable(0, 1));

    const trace2d::particles::ParticleLifetimeTransition transition =
        trace2d::particles::AdvanceExistingParticleLifetime(0, 1);

    EXPECT_EQ(transition.ageAfterUpdate, 1U);
    EXPECT_TRUE(transition.expiresBeforeObservation);
    EXPECT_FALSE(trace2d::particles::IsParticleObservable(transition.ageAfterUpdate, 1));
}

TEST(ParticleDeterminismTests, ZeroLifetimeIsNeverObservable)
{
    EXPECT_FALSE(trace2d::particles::IsParticleObservable(0, 0));

    const trace2d::particles::ParticleLifetimeTransition transition =
        trace2d::particles::AdvanceExistingParticleLifetime(0, 0);
    EXPECT_EQ(transition.ageAfterUpdate, 0U);
    EXPECT_TRUE(transition.expiresBeforeObservation);
}

TEST(ParticleDeterminismTests, FixedRandomKeyHasExactPortableIntegerVector)
{
    const ParticleRandomKey key = MakeKey(42, ParticleRandomChannel::SpawnPositionX);

    EXPECT_EQ(trace2d::particles::ParticleRandomBits(key), 0xE2B5E492311156F8ULL);
    EXPECT_EQ(trace2d::particles::ParticleRandomU32(key), 0xE2B5E492U);
}

TEST(ParticleDeterminismTests, UnitFloatUsesExactTop24BitMapping)
{
    const float value = trace2d::particles::ParticleRandomUnitFloat(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX));

    EXPECT_GE(value, 0.0F);
    EXPECT_LT(value, 1.0F);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(value), 0x3F62B5E4U);
}

TEST(ParticleDeterminismTests, FloatRangeUsesDocumentedUnitMapping)
{
    const float value = trace2d::particles::ParticleRandomFloatRange(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX),
        -2.0F,
        6.0F);

    EXPECT_GE(value, -2.0F);
    EXPECT_LT(value, 6.0F);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(value), 0x40A2B5E4U);
}

TEST(ParticleDeterminismTests, SpawnOrdinalIsStatelessAndIsolatesParticleValues)
{
    const std::uint64_t ordinal41 = trace2d::particles::ParticleRandomBits(
        MakeKey(41, ParticleRandomChannel::SpawnPositionX));
    const std::uint64_t ordinal42 = trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX));
    const std::uint64_t ordinal43 = trace2d::particles::ParticleRandomBits(
        MakeKey(43, ParticleRandomChannel::SpawnPositionX));

    EXPECT_EQ(ordinal41, 0xE16BD29AB97AA33EULL);
    EXPECT_EQ(ordinal42, 0xE2B5E492311156F8ULL);
    EXPECT_EQ(ordinal43, 0xFC88FAA9E54AB4C9ULL);

    static_cast<void>(trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::Angle)));

    EXPECT_EQ(
        trace2d::particles::ParticleRandomBits(
            MakeKey(41, ParticleRandomChannel::SpawnPositionX)),
        ordinal41);
    EXPECT_EQ(
        trace2d::particles::ParticleRandomBits(
            MakeKey(43, ParticleRandomChannel::SpawnPositionX)),
        ordinal43);
}

TEST(ParticleDeterminismTests, EmitterIdentityIsolatesRandomValues)
{
    const std::uint64_t firstEmitter = trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX));
    const std::uint64_t secondEmitter = trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX, TestEmitter ^ 1ULL));

    EXPECT_EQ(firstEmitter, 0xE2B5E492311156F8ULL);
    EXPECT_EQ(secondEmitter, 0xF6D6712980EC50FAULL);
    EXPECT_NE(firstEmitter, secondEmitter);

    EXPECT_EQ(
        trace2d::particles::ParticleRandomBits(
            MakeKey(42, ParticleRandomChannel::SpawnPositionX)),
        firstEmitter);
}

TEST(ParticleDeterminismTests, ExplicitChannelIdsPreventChannelInsertionFromShiftingValues)
{
    EXPECT_EQ(static_cast<std::uint32_t>(ParticleRandomChannel::SpawnPositionX), 0x00010001U);
    EXPECT_EQ(static_cast<std::uint32_t>(ParticleRandomChannel::Lifetime), 0x00020001U);
    EXPECT_EQ(static_cast<std::uint32_t>(ParticleRandomChannel::ColorA), 0x00060004U);

    const std::uint64_t positionBefore = trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::SpawnPositionX));
    const std::uint64_t lifetimeBefore = trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::Lifetime));

    static_cast<void>(trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::ColorR)));
    static_cast<void>(trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::ColorG)));
    static_cast<void>(trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::ColorB)));
    static_cast<void>(trace2d::particles::ParticleRandomBits(
        MakeKey(42, ParticleRandomChannel::ColorA)));

    EXPECT_EQ(
        trace2d::particles::ParticleRandomBits(
            MakeKey(42, ParticleRandomChannel::SpawnPositionX)),
        positionBefore);
    EXPECT_EQ(
        trace2d::particles::ParticleRandomBits(
            MakeKey(42, ParticleRandomChannel::Lifetime)),
        lifetimeBefore);
}

TEST(ParticleDeterminismTests, ReusingSameSeedReproducesWholeKeyedSequence)
{
    const std::array<std::uint64_t, 4> firstRun = BuildResetSequence(TestSeed);
    const std::array<std::uint64_t, 4> differentSeed = BuildResetSequence(TestSeed ^ 1ULL);
    const std::array<std::uint64_t, 4> resetRun = BuildResetSequence(TestSeed);

    EXPECT_EQ(firstRun, resetRun);
    EXPECT_NE(firstRun, differentSeed);
}
} // namespace
