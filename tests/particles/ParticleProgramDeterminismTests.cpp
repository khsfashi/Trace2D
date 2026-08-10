#include <trace2d/particles/ParticleProgram.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace
{
[[nodiscard]] trace2d::particles::ParticleEffectAsset MakeEffect()
{
    using namespace trace2d::particles;

    ParticleEffectAsset effect{};
    effect.id = "tests/particles/deterministic.trace2d.particle.toml";
    effect.semanticId = "deterministic_cost_fixture";
    effect.backend = ParticleEffectBackend::Cpu;
    effect.lifecycle = ParticleEffectLifecycle{4U, false, true};
    effect.definition.maxParticles = 16U;
    effect.definition.periodicStartFrame = 0U;
    effect.definition.periodicCount = 1U;
    effect.definition.periodicEveryFrames = 1U;
    effect.definition.spawnShape.type = ParticleSpawnShapeType::Point;
    effect.definition.lifetimeFrames = ParticleUIntRange{4U, 4U};
    effect.definition.speed = ParticleFloatRange{0.0F, 0.0F};
    effect.definition.angleRadians = ParticleFloatRange{0.0F, 0.0F};
    effect.definition.acceleration = ParticleVec2{0.0F, 0.0F};
    effect.definition.initialSize = ParticleFloatRange{1.0F, 1.0F};
    effect.definition.endSizeMultiplier = 1.0F;
    effect.definition.rotationRadians = ParticleFloatRange{0.0F, 0.0F};
    effect.definition.angularVelocityRadiansPerFrame = ParticleFloatRange{0.0F, 0.0F};
    effect.definition.initialColor = ParticleColorRange{
        ParticleColor{1.0F, 1.0F, 1.0F, 1.0F},
        ParticleColor{1.0F, 1.0F, 1.0F, 1.0F},
    };
    effect.definition.endColor = ParticleColor{1.0F, 1.0F, 1.0F, 1.0F};
    effect.definition.spriteChoiceCount = 1U;
    effect.definition.simulationSpace = ParticleSimulationSpace::Local;
    effect.spriteReferences.push_back("textures/particles/fixture.png");
    return effect;
}

[[nodiscard]] trace2d::particles::ParticleStructuralCostReport Analyze(
    const trace2d::particles::ParticleProgram& program)
{
    using namespace trace2d::particles;

    ParticleEmitter2D emitter{};
    const ParticleEmitter2DPrepareResult prepare =
        PrepareParticleProgramCpuEmitter(program, 0xC0FFEEU, 0x1234U, emitter);
    EXPECT_TRUE(prepare.Ok());
    if (!prepare.Ok())
    {
        return {};
    }

    emitter.Restart();
    ParticleCostAccumulator accumulator{};
    accumulator.Reset(emitter);
    for (std::uint32_t frame = 0U; frame < 4U; ++frame)
    {
        EXPECT_TRUE(emitter.Step());
        accumulator.ObserveAfterStep(emitter);
    }
    return BuildParticleStructuralCostReport(program, accumulator.Observation(emitter));
}

TEST(ParticleProgramDeterminismTests, SameProgramAndWorkloadProduceIdenticalStructuralReport)
{
    const trace2d::particles::ParticleProgram program =
        trace2d::particles::CompileParticleProgram(MakeEffect());

    const trace2d::particles::ParticleStructuralCostReport first = Analyze(program);
    const trace2d::particles::ParticleStructuralCostReport second = Analyze(program);

    EXPECT_EQ(first, second);
    EXPECT_EQ(first.programFingerprint, program.fingerprint);
    EXPECT_EQ(first.observedFrames, 4U);
    EXPECT_EQ(first.steadyStateSimulationAllocations, 0U);
}
} // namespace
