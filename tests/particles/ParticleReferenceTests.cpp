#include <trace2d/particles/ParticleReference.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
using trace2d::particles::ParticleBurst;
using trace2d::particles::ParticleColor;
using trace2d::particles::ParticleColorRange;
using trace2d::particles::ParticleFloatRange;
using trace2d::particles::ParticleReferenceDefinition;
using trace2d::particles::ParticleReferenceEmitter;
using trace2d::particles::ParticleReferenceError;
using trace2d::particles::ParticleReferenceLimits;
using trace2d::particles::ParticleReferenceParticle;
using trace2d::particles::ParticleSimulationSpace;
using trace2d::particles::ParticleSpawnShapeType;
using trace2d::particles::ParticleUIntRange;
using trace2d::particles::ParticleVec2;

[[nodiscard]] ParticleReferenceDefinition MakeRichDefinition() noexcept
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 64;
    definition.globalSeed = 0x0123456789ABCDEFULL;
    definition.emitterStableId = 0xFEDCBA9876543210ULL;
    definition.periodicStartFrame = 0;
    definition.periodicCount = 3;
    definition.periodicEveryFrames = 2;
    definition.spawnShape.type = ParticleSpawnShapeType::Circle;
    definition.spawnShape.offset = ParticleVec2{2.0F, -1.0F};
    definition.spawnShape.circleRadius = 4.0F;
    definition.lifetimeFrames = ParticleUIntRange{2, 6};
    definition.speed = ParticleFloatRange{0.5F, 3.0F};
    definition.angleRadians = ParticleFloatRange{-3.0F, 3.0F};
    definition.acceleration = ParticleVec2{0.0F, -0.1F};
    definition.initialSize = ParticleFloatRange{0.5F, 2.0F};
    definition.endSizeMultiplier = 0.25F;
    definition.rotationRadians = ParticleFloatRange{-1.0F, 1.0F};
    definition.angularVelocityRadiansPerFrame = ParticleFloatRange{-0.2F, 0.2F};
    definition.initialColor = ParticleColorRange{
        ParticleColor{0.2F, 0.3F, 0.4F, 0.5F},
        ParticleColor{1.0F, 1.0F, 1.0F, 1.0F},
    };
    definition.endColor = ParticleColor{0.1F, 0.2F, 0.3F, 0.0F};
    definition.spriteChoiceCount = 5;
    definition.simulationSpace = ParticleSimulationSpace::World;
    return definition;
}

[[nodiscard]] std::vector<ParticleReferenceParticle> Snapshot(
    const ParticleReferenceEmitter& emitter)
{
    std::vector<ParticleReferenceParticle> particles;
    particles.reserve(emitter.AliveCount());
    for (std::uint32_t index = 0; index < emitter.AliveCount(); ++index)
    {
        ParticleReferenceParticle particle{};
        EXPECT_TRUE(emitter.TryGetParticle(index, particle));
        particles.push_back(particle);
    }
    return particles;
}

TEST(ParticleReferenceTests, RequiresBoundedValidCapacityBeforeAllocating)
{
    ParticleReferenceEmitter emitter;
    ParticleReferenceDefinition definition = MakeRichDefinition();
    ParticleReferenceLimits limits{};
    limits.maxParticlesPerEmitter = 8;

    definition.maxParticles = 0;
    EXPECT_EQ(
        emitter.Prepare(definition, {}, limits).error,
        ParticleReferenceError::ZeroCapacity);

    definition.maxParticles = 9;
    EXPECT_EQ(
        emitter.Prepare(definition, {}, limits).error,
        ParticleReferenceError::CapacityExceedsLimit);
    EXPECT_FALSE(emitter.IsPrepared());
}

TEST(ParticleReferenceTests, RejectsUnsafeOrAmbiguousEmissionConfiguration)
{
    ParticleReferenceEmitter emitter;
    ParticleReferenceDefinition definition = MakeRichDefinition();

    definition.periodicCount = 1;
    definition.periodicEveryFrames = 0;
    EXPECT_EQ(
        emitter.Prepare(definition, {}).error,
        ParticleReferenceError::InvalidPeriodicEmission);

    definition = MakeRichDefinition();
    const std::array unorderedBursts{
        ParticleBurst{4, 1},
        ParticleBurst{3, 1},
    };
    EXPECT_EQ(
        emitter.Prepare(definition, unorderedBursts).error,
        ParticleReferenceError::BurstsNotOrdered);

    definition = MakeRichDefinition();
    ParticleReferenceLimits limits{};
    limits.maxSpawnAttemptsPerFrame = 4;
    const std::array overloadedBursts{
        ParticleBurst{0, 2},
    };
    EXPECT_EQ(
        emitter.Prepare(definition, overloadedBursts, limits).error,
        ParticleReferenceError::SpawnAttemptsPerFrameExceedLimit);
}

TEST(ParticleReferenceTests, BurstThenPeriodicEmissionUsesStableSpawnAttemptOrdinals)
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 8;
    definition.globalSeed = 1;
    definition.emitterStableId = 2;
    definition.periodicStartFrame = 0;
    definition.periodicCount = 1;
    definition.periodicEveryFrames = 2;
    definition.lifetimeFrames = ParticleUIntRange{4, 4};

    const std::array bursts{
        ParticleBurst{0, 2},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, bursts).Ok());
    ASSERT_TRUE(emitter.Step());

    ASSERT_EQ(emitter.AliveCount(), 3U);
    for (std::uint32_t index = 0; index < emitter.AliveCount(); ++index)
    {
        ParticleReferenceParticle particle{};
        ASSERT_TRUE(emitter.TryGetParticle(index, particle));
        EXPECT_EQ(particle.spawnOrdinal, index);
        EXPECT_EQ(particle.ageFrames, 0U);
    }
    EXPECT_EQ(emitter.NextSpawnOrdinal(), 3U);
}

TEST(ParticleReferenceTests, LifetimeBoundaryExpiresBeforeObservationAndReusesDenseStorage)
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 2;
    definition.globalSeed = 7;
    definition.emitterStableId = 11;
    definition.lifetimeFrames = ParticleUIntRange{1, 1};

    const std::array bursts{
        ParticleBurst{0, 4},
        ParticleBurst{1, 2},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, bursts).Ok());

    ASSERT_TRUE(emitter.Step());
    ASSERT_EQ(emitter.AliveCount(), 2U);
    EXPECT_EQ(emitter.Counters().spawnAttempts, 4U);
    EXPECT_EQ(emitter.Counters().spawned, 2U);
    EXPECT_EQ(emitter.Counters().dropped, 2U);
    EXPECT_EQ(emitter.NextSpawnOrdinal(), 4U);

    ParticleReferenceParticle particle{};
    ASSERT_TRUE(emitter.TryGetParticle(0, particle));
    EXPECT_EQ(particle.spawnOrdinal, 0U);
    ASSERT_TRUE(emitter.TryGetParticle(1, particle));
    EXPECT_EQ(particle.spawnOrdinal, 1U);

    ASSERT_TRUE(emitter.Step());
    ASSERT_EQ(emitter.AliveCount(), 2U);
    EXPECT_EQ(emitter.Counters().expired, 2U);
    EXPECT_EQ(emitter.Counters().spawnAttempts, 6U);
    EXPECT_EQ(emitter.Counters().spawned, 4U);
    EXPECT_EQ(emitter.Counters().dropped, 2U);
    EXPECT_EQ(emitter.NextSpawnOrdinal(), 6U);

    ASSERT_TRUE(emitter.TryGetParticle(0, particle));
    EXPECT_EQ(particle.spawnOrdinal, 4U);
    EXPECT_EQ(particle.ageFrames, 0U);
    ASSERT_TRUE(emitter.TryGetParticle(1, particle));
    EXPECT_EQ(particle.spawnOrdinal, 5U);
    EXPECT_EQ(particle.ageFrames, 0U);
}

TEST(ParticleReferenceTests, StableCompactionPreservesSurvivorSpawnOrder)
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 6;
    definition.globalSeed = 0x11223344ULL;
    definition.emitterStableId = 0x55667788ULL;
    definition.lifetimeFrames = ParticleUIntRange{1, 4};

    const std::array bursts{
        ParticleBurst{0, 6},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, bursts).Ok());
    ASSERT_TRUE(emitter.Step());

    std::vector<trace2d::particles::ParticleSpawnOrdinal> expectedSurvivors;
    for (std::uint32_t index = 0; index < emitter.AliveCount(); ++index)
    {
        ParticleReferenceParticle particle{};
        ASSERT_TRUE(emitter.TryGetParticle(index, particle));
        if (particle.lifetimeFrames > 1U)
        {
            expectedSurvivors.push_back(particle.spawnOrdinal);
        }
    }
    ASSERT_FALSE(expectedSurvivors.empty());
    ASSERT_LT(expectedSurvivors.size(), 6U);

    ASSERT_TRUE(emitter.Step());
    ASSERT_EQ(emitter.AliveCount(), expectedSurvivors.size());
    for (std::uint32_t index = 0; index < emitter.AliveCount(); ++index)
    {
        ParticleReferenceParticle particle{};
        ASSERT_TRUE(emitter.TryGetParticle(index, particle));
        EXPECT_EQ(particle.spawnOrdinal, expectedSurvivors[index]);
        EXPECT_EQ(particle.ageFrames, 1U);
    }
}

TEST(ParticleReferenceTests, ResetReplaysEverySupportedRichFieldExactly)
{
    const std::array bursts{
        ParticleBurst{1, 2},
        ParticleBurst{4, 4},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(MakeRichDefinition(), bursts).Ok());

    for (std::uint32_t frame = 0; frame < 8U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
    }
    const std::vector<ParticleReferenceParticle> firstParticles = Snapshot(emitter);
    const trace2d::particles::ParticleReferenceCounters firstCounters = emitter.Counters();

    emitter.Reset();
    EXPECT_EQ(emitter.NextFrameIndex(), 0U);
    EXPECT_EQ(emitter.NextSpawnOrdinal(), 0U);
    EXPECT_EQ(emitter.AliveCount(), 0U);

    for (std::uint32_t frame = 0; frame < 8U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
    }

    const std::vector<ParticleReferenceParticle> replayParticles = Snapshot(emitter);
    ASSERT_EQ(firstParticles.size(), replayParticles.size());
    for (std::size_t index = 0; index < firstParticles.size(); ++index)
    {
        EXPECT_TRUE(firstParticles[index] == replayParticles[index]);
    }
    EXPECT_TRUE(firstCounters == emitter.Counters());
}

TEST(ParticleReferenceTests, RichStateIsAvailableByStableSpawnOrdinalWithoutRenderer)
{
    ParticleReferenceDefinition definition = MakeRichDefinition();
    definition.periodicCount = 0;
    definition.periodicEveryFrames = 0;
    const std::array bursts{
        ParticleBurst{0, 1},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, bursts).Ok());
    ASSERT_TRUE(emitter.Step());

    ParticleReferenceParticle byIndex{};
    ParticleReferenceParticle byOrdinal{};
    ASSERT_TRUE(emitter.TryGetParticle(0, byIndex));
    ASSERT_TRUE(emitter.TryGetParticleBySpawnOrdinal(byIndex.spawnOrdinal, byOrdinal));

    EXPECT_TRUE(byIndex == byOrdinal);
    EXPECT_EQ(byIndex.ageFrames, 0U);
    EXPECT_GE(byIndex.lifetimeFrames, definition.lifetimeFrames.minValue);
    EXPECT_LE(byIndex.lifetimeFrames, definition.lifetimeFrames.maxValue);
    EXPECT_TRUE(byIndex.acceleration == definition.acceleration);
    EXPECT_GE(byIndex.initialSize, definition.initialSize.minValue);
    EXPECT_LE(byIndex.initialSize, definition.initialSize.maxValue);
    EXPECT_EQ(byIndex.size, byIndex.initialSize);
    EXPECT_LT(byIndex.spriteIndex, definition.spriteChoiceCount);
    EXPECT_EQ(byIndex.simulationSpace, ParticleSimulationSpace::World);
    EXPECT_FALSE(emitter.TryGetParticleBySpawnOrdinal(9999U, byOrdinal));
}

TEST(ParticleReferenceTests, ExistingParticleUpdateOrderIsAccelerationVelocityPositionThenLifetime)
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 1;
    definition.lifetimeFrames = ParticleUIntRange{3, 3};
    definition.speed = ParticleFloatRange{2.0F, 2.0F};
    definition.angleRadians = ParticleFloatRange{0.0F, 0.0F};
    definition.acceleration = ParticleVec2{1.0F, -1.0F};
    definition.initialSize = ParticleFloatRange{2.0F, 2.0F};
    definition.endSizeMultiplier = 0.5F;
    definition.initialColor = ParticleColorRange{
        ParticleColor{1.0F, 1.0F, 1.0F, 1.0F},
        ParticleColor{1.0F, 1.0F, 1.0F, 1.0F},
    };
    definition.endColor = ParticleColor{0.0F, 0.0F, 0.0F, 0.0F};

    const std::array bursts{
        ParticleBurst{0, 1},
    };

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, bursts).Ok());
    ASSERT_TRUE(emitter.Step());

    ParticleReferenceParticle particle{};
    ASSERT_TRUE(emitter.TryGetParticle(0, particle));
    const ParticleVec2 initialPosition{0.0F, 0.0F};
    const ParticleVec2 initialVelocity{2.0F, 0.0F};
    EXPECT_TRUE(particle.position == initialPosition);
    EXPECT_TRUE(particle.velocity == initialVelocity);
    EXPECT_EQ(particle.ageFrames, 0U);
    EXPECT_EQ(particle.size, 2.0F);

    ASSERT_TRUE(emitter.Step());
    ASSERT_TRUE(emitter.TryGetParticle(0, particle));
    const ParticleVec2 updatedVelocity{3.0F, -1.0F};
    const ParticleVec2 updatedPosition{3.0F, -1.0F};
    const ParticleColor updatedColor{0.5F, 0.5F, 0.5F, 0.5F};
    EXPECT_TRUE(particle.velocity == updatedVelocity);
    EXPECT_TRUE(particle.position == updatedPosition);
    EXPECT_EQ(particle.ageFrames, 1U);
    EXPECT_EQ(particle.size, 1.5F);
    EXPECT_TRUE(particle.color == updatedColor);
}

TEST(ParticleReferenceTests, UnrelatedEmitterSteppingDoesNotPerturbReferenceReplay)
{
    const std::array bursts{
        ParticleBurst{0, 4},
        ParticleBurst{3, 2},
    };

    ParticleReferenceDefinition primaryDefinition = MakeRichDefinition();
    primaryDefinition.periodicCount = 1;
    primaryDefinition.periodicEveryFrames = 1;

    ParticleReferenceEmitter standalone;
    ASSERT_TRUE(standalone.Prepare(primaryDefinition, bursts).Ok());
    for (std::uint32_t frame = 0; frame < 7U; ++frame)
    {
        ASSERT_TRUE(standalone.Step());
    }
    const std::vector<ParticleReferenceParticle> expected = Snapshot(standalone);

    ParticleReferenceEmitter interleaved;
    ASSERT_TRUE(interleaved.Prepare(primaryDefinition, bursts).Ok());

    ParticleReferenceDefinition unrelatedDefinition = primaryDefinition;
    unrelatedDefinition.emitterStableId ^= 0x55AA55AAULL;
    ParticleReferenceEmitter unrelated;
    ASSERT_TRUE(unrelated.Prepare(unrelatedDefinition, bursts).Ok());

    for (std::uint32_t frame = 0; frame < 7U; ++frame)
    {
        ASSERT_TRUE(unrelated.Step());
        ASSERT_TRUE(interleaved.Step());
        ASSERT_TRUE(unrelated.Step());
    }

    const std::vector<ParticleReferenceParticle> actual = Snapshot(interleaved);
    ASSERT_EQ(expected.size(), actual.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_TRUE(expected[index] == actual[index]);
    }
}

TEST(ParticleReferenceTests, RepresentativeCapacityHasExactPayloadAccountingAndStablePreparedStorage)
{
    ParticleReferenceDefinition definition{};
    definition.maxParticles = 4'096;
    definition.globalSeed = 9;
    definition.emitterStableId = 10;
    definition.periodicStartFrame = 0;
    definition.periodicCount = 32;
    definition.periodicEveryFrames = 1;
    definition.lifetimeFrames = ParticleUIntRange{120, 120};

    ParticleReferenceEmitter emitter;
    ASSERT_TRUE(emitter.Prepare(definition, {}).Ok());

    const trace2d::particles::ParticleReferenceMemoryReport before = emitter.MemoryReport();
    EXPECT_EQ(before.bytesPerParticlePayload, 92U);
    EXPECT_EQ(before.particleStorageBytes, 376'832U);
    EXPECT_EQ(before.burstScheduleBytes, 0U);
    EXPECT_EQ(before.preparedPayloadBytes, 376'832U);
    EXPECT_EQ(before.storageBlockCount, 13U);
    EXPECT_EQ(before.steadyStateSimulationAllocations, 0U);

    for (std::uint32_t frame = 0; frame < 240U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
    }

    const trace2d::particles::ParticleReferenceMemoryReport after = emitter.MemoryReport();
    EXPECT_TRUE(before == after);
    EXPECT_LE(emitter.AliveCount(), definition.maxParticles);
    EXPECT_GT(emitter.Counters().updated, 0U);
}
} // namespace
