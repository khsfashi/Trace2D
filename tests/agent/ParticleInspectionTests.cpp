#include <trace2d/agent/Inspection.hpp>
#include <trace2d/particles/ParticleEffect.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

namespace
{
[[nodiscard]] std::shared_ptr<const trace2d::particles::ParticleEffectAsset> MakeEffect()
{
    auto effect = std::make_shared<trace2d::particles::ParticleEffectAsset>();
    effect->id = "effects/agent_verification.trace2d.particle.toml";
    effect->semanticId = "agent_verification";
    effect->backend = trace2d::particles::ParticleEffectBackend::Cpu;
    effect->lifecycle.durationFrames = 32U;
    effect->lifecycle.loop = false;
    effect->lifecycle.playOnLoad = true;

    trace2d::particles::ParticleReferenceDefinition& definition = effect->definition;
    definition.maxParticles = 8U;
    definition.periodicStartFrame = 0U;
    definition.periodicCount = 3U;
    definition.periodicEveryFrames = 1U;
    definition.spawnShape.type = trace2d::particles::ParticleSpawnShapeType::Point;
    definition.spawnShape.offset = {1.0F, 2.0F};
    definition.lifetimeFrames = {8U, 8U};
    definition.speed = {2.0F, 2.0F};
    definition.angleRadians = {0.0F, 0.0F};
    definition.acceleration = {0.25F, -0.5F};
    definition.initialSize = {2.0F, 2.0F};
    definition.endSizeMultiplier = 0.5F;
    definition.rotationRadians = {0.5F, 0.5F};
    definition.angularVelocityRadiansPerFrame = {0.1F, 0.1F};
    definition.initialColor.minValue = {0.2F, 0.3F, 0.4F, 0.8F};
    definition.initialColor.maxValue = definition.initialColor.minValue;
    definition.endColor = {0.5F, 0.6F, 0.7F, 0.1F};
    definition.spriteChoiceCount = 2U;
    definition.simulationSpace = trace2d::particles::ParticleSimulationSpace::World;
    effect->spriteReferences = {"textures/a.png", "textures/b.png"};
    return effect;
}

[[nodiscard]] trace2d::particles::ParticleEmitter2D MakeEmitter(
    const std::shared_ptr<const trace2d::particles::ParticleEffectAsset>& effect,
    const std::uint64_t seed,
    const trace2d::particles::ParticleEmitterStableId stableId)
{
    trace2d::particles::ParticleEmitter2D emitter{};
    const trace2d::particles::ParticleEmitter2DPrepareResult prepare =
        emitter.Prepare(effect, seed, stableId);
    EXPECT_TRUE(prepare.Ok());
    return emitter;
}

TEST(ParticleInspectionTests, AggregateInspectionIsDeterministicAndEmitterLocal)
{
    const auto effect = MakeEffect();
    auto emitter = MakeEmitter(effect, 1234U, 77U);
    ASSERT_TRUE(emitter.Step());
    ASSERT_TRUE(emitter.Step());

    const trace2d::agent::AgentFacade facade{};
    const trace2d::agent::ParticleEmitterBinding binding{"fx_anchor", &emitter};
    const trace2d::agent::ParticleEmitterInspectionResult first =
        facade.InspectParticleEmitter(binding);
    const trace2d::agent::ParticleEmitterInspectionResult second =
        facade.InspectParticleEmitter(binding);

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(first.snapshot.has_value());
    ASSERT_TRUE(second.Succeeded());
    ASSERT_TRUE(second.snapshot.has_value());
    EXPECT_EQ(first.snapshot, second.snapshot);
    EXPECT_EQ(first.snapshot->entitySemanticId, "fx_anchor");
    EXPECT_EQ(first.snapshot->effectSemanticId, "agent_verification");
    EXPECT_EQ(first.snapshot->effectAssetId, "effects/agent_verification.trace2d.particle.toml");
    EXPECT_EQ(first.snapshot->emitterStableId, 77U);
    EXPECT_TRUE(first.snapshot->prepared);
    EXPECT_TRUE(first.snapshot->playing);
    EXPECT_EQ(first.snapshot->cycleFrame, 2U);
    EXPECT_EQ(first.snapshot->nextSimulationFrame, 2U);
    EXPECT_EQ(first.snapshot->aliveCount, 6U);
    EXPECT_EQ(first.snapshot->capacity, 8U);
    EXPECT_EQ(first.snapshot->emittedTotal, 6U);
    EXPECT_EQ(first.snapshot->droppedTotal, 0U);

    auto unrelatedEmitter = MakeEmitter(effect, 9999U, 500U);
    ASSERT_TRUE(unrelatedEmitter.Step());
    ASSERT_TRUE(unrelatedEmitter.Step());
    const trace2d::agent::ParticleEmitterInspectionResult afterUnrelated =
        facade.InspectParticleEmitter(binding);
    ASSERT_TRUE(afterUnrelated.Succeeded());
    ASSERT_TRUE(afterUnrelated.snapshot.has_value());
    EXPECT_EQ(afterUnrelated.snapshot->stateFingerprint, first.snapshot->stateFingerprint);
}

TEST(ParticleInspectionTests, BoundedDetailExposesEveryV1ReferencePropertyInSpawnOrder)
{
    const auto effect = MakeEffect();
    auto emitter = MakeEmitter(effect, 44U, 9U);
    ASSERT_TRUE(emitter.Step());

    const trace2d::agent::AgentFacade facade{};
    const trace2d::agent::ParticleEmitterBinding binding{"fx_anchor", &emitter};
    const trace2d::agent::ParticleDetailInspectionResult detail =
        facade.InspectParticles(binding, 1U, 1U);

    ASSERT_TRUE(detail.Succeeded());
    ASSERT_TRUE(detail.snapshot.has_value());
    EXPECT_EQ(detail.snapshot->totalAlive, 3U);
    EXPECT_EQ(detail.snapshot->offset, 1U);
    EXPECT_EQ(detail.snapshot->requestedLimit, 1U);
    ASSERT_EQ(detail.snapshot->particles.size(), 1U);

    trace2d::particles::ParticleReferenceParticle referenceParticle{};
    ASSERT_TRUE(emitter.Reference().TryGetParticle(1U, referenceParticle));
    const trace2d::agent::ParticleStateSnapshot& particle = detail.snapshot->particles[0];
    EXPECT_EQ(particle.spawnOrdinal, referenceParticle.spawnOrdinal);
    EXPECT_FLOAT_EQ(particle.position.x, referenceParticle.position.x);
    EXPECT_FLOAT_EQ(particle.position.y, referenceParticle.position.y);
    EXPECT_FLOAT_EQ(particle.velocity.x, referenceParticle.velocity.x);
    EXPECT_FLOAT_EQ(particle.velocity.y, referenceParticle.velocity.y);
    EXPECT_FLOAT_EQ(particle.acceleration.x, referenceParticle.acceleration.x);
    EXPECT_FLOAT_EQ(particle.acceleration.y, referenceParticle.acceleration.y);
    EXPECT_EQ(particle.ageFrames, referenceParticle.ageFrames);
    EXPECT_EQ(particle.lifetimeFrames, referenceParticle.lifetimeFrames);
    EXPECT_FLOAT_EQ(particle.initialSize, referenceParticle.initialSize);
    EXPECT_FLOAT_EQ(particle.size, referenceParticle.size);
    EXPECT_FLOAT_EQ(particle.rotationRadians, referenceParticle.rotationRadians);
    EXPECT_FLOAT_EQ(
        particle.angularVelocityRadiansPerFrame,
        referenceParticle.angularVelocityRadiansPerFrame);
    EXPECT_FLOAT_EQ(particle.initialColor.r, referenceParticle.initialColor.r);
    EXPECT_FLOAT_EQ(particle.initialColor.g, referenceParticle.initialColor.g);
    EXPECT_FLOAT_EQ(particle.initialColor.b, referenceParticle.initialColor.b);
    EXPECT_FLOAT_EQ(particle.initialColor.a, referenceParticle.initialColor.a);
    EXPECT_FLOAT_EQ(particle.color.r, referenceParticle.color.r);
    EXPECT_FLOAT_EQ(particle.color.g, referenceParticle.color.g);
    EXPECT_FLOAT_EQ(particle.color.b, referenceParticle.color.b);
    EXPECT_FLOAT_EQ(particle.color.a, referenceParticle.color.a);
    EXPECT_EQ(particle.spriteIndex, referenceParticle.spriteIndex);
    EXPECT_EQ(
        particle.simulationSpace,
        trace2d::agent::ParticleSimulationSpaceSnapshot::World);

    const trace2d::agent::ParticleSingleInspectionResult byOrdinal =
        facade.InspectParticle(binding, particle.spawnOrdinal);
    ASSERT_TRUE(byOrdinal.Succeeded());
    ASSERT_TRUE(byOrdinal.particle.has_value());
    EXPECT_EQ(*byOrdinal.particle, particle);

    const trace2d::agent::ParticleDetailInspectionResult ordered =
        facade.InspectParticles(binding, 0U, 3U);
    ASSERT_TRUE(ordered.Succeeded());
    ASSERT_TRUE(ordered.snapshot.has_value());
    ASSERT_EQ(ordered.snapshot->particles.size(), 3U);
    EXPECT_LT(ordered.snapshot->particles[0].spawnOrdinal, ordered.snapshot->particles[1].spawnOrdinal);
    EXPECT_LT(ordered.snapshot->particles[1].spawnOrdinal, ordered.snapshot->particles[2].spawnOrdinal);
}

TEST(ParticleInspectionTests, InvalidRangesAndOrdinalsReturnStableStructuredErrors)
{
    const auto effect = MakeEffect();
    auto emitter = MakeEmitter(effect, 1U, 2U);
    ASSERT_TRUE(emitter.Step());

    const trace2d::agent::AgentFacade facade{};
    const trace2d::agent::ParticleEmitterBinding binding{"fx_anchor", &emitter};

    const auto zeroLimit = facade.InspectParticles(binding, 0U, 0U);
    ASSERT_TRUE(zeroLimit.error.has_value());
    EXPECT_EQ(zeroLimit.error->code, trace2d::agent::ParticleInspectionErrorCode::InvalidRange);
    EXPECT_EQ(trace2d::agent::ToString(zeroLimit.error->code), "invalid_range");

    const auto tooLarge = facade.InspectParticles(
        binding,
        0U,
        trace2d::agent::MaxParticleInspectionCount + 1U);
    ASSERT_TRUE(tooLarge.error.has_value());
    EXPECT_EQ(tooLarge.error->code, trace2d::agent::ParticleInspectionErrorCode::InvalidRange);

    const auto badOffset = facade.InspectParticles(binding, emitter.Reference().AliveCount() + 1U, 1U);
    ASSERT_TRUE(badOffset.error.has_value());
    EXPECT_EQ(badOffset.error->code, trace2d::agent::ParticleInspectionErrorCode::InvalidRange);

    const auto missing = facade.InspectParticle(binding, 99'999U);
    ASSERT_TRUE(missing.error.has_value());
    EXPECT_EQ(missing.error->code, trace2d::agent::ParticleInspectionErrorCode::ParticleNotFound);
    EXPECT_EQ(trace2d::agent::ToString(missing.error->code), "particle_not_found");

    const trace2d::agent::ParticleEmitterBinding unavailable{"fx_anchor", nullptr};
    const auto noEmitter = facade.InspectParticleEmitter(unavailable);
    ASSERT_TRUE(noEmitter.error.has_value());
    EXPECT_EQ(noEmitter.error->code, trace2d::agent::ParticleInspectionErrorCode::EmitterUnavailable);

    trace2d::particles::ParticleEmitter2D unprepared{};
    const trace2d::agent::ParticleEmitterBinding unpreparedBinding{"fx_anchor", &unprepared};
    const auto notPrepared = facade.InspectParticleEmitter(unpreparedBinding);
    ASSERT_TRUE(notPrepared.error.has_value());
    EXPECT_EQ(
        notPrepared.error->code,
        trace2d::agent::ParticleInspectionErrorCode::EmitterNotPrepared);
}

TEST(ParticleInspectionTests, TypedAssertionsReportExpectedObservedAndExactContext)
{
    const auto effect = MakeEffect();
    auto emitter = MakeEmitter(effect, 123U, 456U);
    ASSERT_TRUE(emitter.Step());

    trace2d::particles::ParticleReferenceParticle referenceParticle{};
    ASSERT_TRUE(emitter.Reference().TryGetParticle(0U, referenceParticle));

    const trace2d::agent::AgentFacade facade{};
    const trace2d::agent::ParticleEmitterBinding binding{"fx_anchor", &emitter};

    const trace2d::agent::ParticleAssertion aliveAssertion{
        .field = trace2d::agent::ParticleAssertionField::AliveCount,
        .spawnOrdinal = std::nullopt,
        .expected = trace2d::agent::ParticleValue::Unsigned(3U),
    };
    const auto aliveResult = facade.AssertParticle(binding, aliveAssertion);
    ASSERT_TRUE(aliveResult.Succeeded());
    ASSERT_TRUE(aliveResult.observed.has_value());
    EXPECT_EQ(aliveResult.observed->unsignedIntegerValue, 3U);

    const trace2d::agent::ParticleAssertion mismatch{
        .field = trace2d::agent::ParticleAssertionField::PositionX,
        .spawnOrdinal = referenceParticle.spawnOrdinal,
        .expected = trace2d::agent::ParticleValue::Float(referenceParticle.position.x + 1.0F),
    };
    const auto mismatchResult = facade.AssertParticle(binding, mismatch);
    ASSERT_FALSE(mismatchResult.Succeeded());
    ASSERT_TRUE(mismatchResult.error.has_value());
    ASSERT_TRUE(mismatchResult.observed.has_value());
    EXPECT_EQ(
        mismatchResult.error->code,
        trace2d::agent::ParticleInspectionErrorCode::StateMismatch);
    EXPECT_EQ(mismatchResult.observed->kind, trace2d::agent::ParticleValueKind::Float);
    EXPECT_FLOAT_EQ(mismatchResult.observed->floatValue, referenceParticle.position.x);
    EXPECT_EQ(mismatchResult.assertion.expected.floatValue, referenceParticle.position.x + 1.0F);
    EXPECT_EQ(mismatchResult.context.entitySemanticId, "fx_anchor");
    EXPECT_EQ(mismatchResult.context.effectSemanticId, "agent_verification");
    EXPECT_EQ(mismatchResult.context.emitterStableId, 456U);
    EXPECT_EQ(mismatchResult.context.nextSimulationFrame, 1U);
    EXPECT_EQ(mismatchResult.context.cycleFrame, 1U);
    EXPECT_EQ(mismatchResult.context.seed, 123U);
    EXPECT_EQ(mismatchResult.context.aliveCount, 3U);
    ASSERT_TRUE(mismatchResult.context.particleDetail.has_value());
    EXPECT_EQ(mismatchResult.context.particleDetail->spawnOrdinal, referenceParticle.spawnOrdinal);

    const trace2d::agent::ParticleAssertion typeMismatch{
        .field = trace2d::agent::ParticleAssertionField::ColorA,
        .spawnOrdinal = referenceParticle.spawnOrdinal,
        .expected = trace2d::agent::ParticleValue::Unsigned(1U),
    };
    const auto typeResult = facade.AssertParticle(binding, typeMismatch);
    ASSERT_FALSE(typeResult.Succeeded());
    ASSERT_TRUE(typeResult.error.has_value());
    EXPECT_EQ(typeResult.error->code, trace2d::agent::ParticleInspectionErrorCode::TypeMismatch);
    ASSERT_TRUE(typeResult.observed.has_value());
    EXPECT_EQ(typeResult.observed->kind, trace2d::agent::ParticleValueKind::Float);

    const trace2d::agent::ParticleAssertion spaceAssertion{
        .field = trace2d::agent::ParticleAssertionField::SimulationSpace,
        .spawnOrdinal = referenceParticle.spawnOrdinal,
        .expected = trace2d::agent::ParticleValue::String("world"),
    };
    EXPECT_TRUE(facade.AssertParticle(binding, spaceAssertion).Succeeded());
}

TEST(ParticleInspectionTests, SameReplayProducesSameExplicitFingerprintWithoutRenderer)
{
    const auto effect = MakeEffect();
    auto first = MakeEmitter(effect, 88U, 12U);
    auto second = MakeEmitter(effect, 88U, 12U);
    for (std::uint32_t frame = 0U; frame < 5U; ++frame)
    {
        ASSERT_TRUE(first.Step());
        ASSERT_TRUE(second.Step());
    }

    const trace2d::agent::AgentFacade facade{};
    const trace2d::agent::ParticleEmitterBinding firstBinding{"fx_anchor", &first};
    const trace2d::agent::ParticleEmitterBinding secondBinding{"fx_anchor", &second};
    const auto firstSnapshot = facade.InspectParticleEmitter(firstBinding);
    const auto secondSnapshot = facade.InspectParticleEmitter(secondBinding);
    ASSERT_TRUE(firstSnapshot.Succeeded());
    ASSERT_TRUE(secondSnapshot.Succeeded());
    ASSERT_TRUE(firstSnapshot.snapshot.has_value());
    ASSERT_TRUE(secondSnapshot.snapshot.has_value());
    EXPECT_EQ(firstSnapshot.snapshot->stateFingerprint, secondSnapshot.snapshot->stateFingerprint);

    const std::uint64_t replayFingerprint = firstSnapshot.snapshot->stateFingerprint;
    first.Reset();
    for (std::uint32_t frame = 0U; frame < 5U; ++frame)
    {
        ASSERT_TRUE(first.Step());
    }
    const auto replay = facade.InspectParticleEmitter(firstBinding);
    ASSERT_TRUE(replay.Succeeded());
    ASSERT_TRUE(replay.snapshot.has_value());
    EXPECT_EQ(replay.snapshot->stateFingerprint, replayFingerprint);

    const trace2d::agent::ParticleAssertion fingerprintAssertion{
        .field = trace2d::agent::ParticleAssertionField::StateFingerprint,
        .spawnOrdinal = std::nullopt,
        .expected = trace2d::agent::ParticleValue::Unsigned(replayFingerprint),
    };
    EXPECT_TRUE(facade.AssertParticle(firstBinding, fingerprintAssertion).Succeeded());
}
} // namespace
