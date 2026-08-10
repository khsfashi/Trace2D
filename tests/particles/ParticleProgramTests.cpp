#include <trace2d/particles/ParticleProgram.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
using trace2d::particles::BuildParticleStructuralCostReport;
using trace2d::particles::CompileParticleGpuArtifact;
using trace2d::particles::CompileParticleProgram;
using trace2d::particles::ParseParticleEffectToml;
using trace2d::particles::ParticleCostAccumulator;
using trace2d::particles::ParticleEffectBackend;
using trace2d::particles::ParticleEmitter2D;
using trace2d::particles::ParticleEmitter2DError;
using trace2d::particles::ParticleGpuCompileError;
using trace2d::particles::ParticleGpuRuntimeFieldKind;
using trace2d::particles::ParticleProgram;
using trace2d::particles::ParticleProgramAttribute;
using trace2d::particles::ParticleProgramOperation;
using trace2d::particles::PrepareParticleProgramCpuEmitter;
using trace2d::particles::SaveParticleEffectToml;

[[nodiscard]] std::string ValidEffectToml()
{
    return R"toml(format_version = 1

[effect]
id = "hit_spark"
backend = "cpu"
max_particles = 64
duration_frames = 12
loop = false
play_on_load = true
simulation_space = "world"

[emission]
start_frame = 1
count = 2
every_frames = 3

[spawn]
shape = "circle"
offset = [2.0, -1.0]
box_half_extents = [0.0, 0.0]
circle_radius = 4.0

[lifetime]
frames = [2, 6]

[motion]
speed = [0.5, 3.0]
angle_radians = [-3.0, 3.0]
acceleration = [0.0, -0.1]

[scale]
initial = [0.5, 2.0]
end_multiplier = 0.25

[rotation]
initial_radians = [-1.0, 1.0]
angular_velocity_radians_per_frame = [-0.2, 0.2]

[color]
initial_min = [0.2, 0.3, 0.4, 0.5]
initial_max = [1.0, 1.0, 1.0, 1.0]
end = [0.1, 0.2, 0.3, 0.0]

[presentation]
blend = "additive"
sprites = ["textures/particles/spark_a.png", "textures/particles/./spark_b.png"]

[[bursts]]
frame = 0
count = 4

[[bursts]]
frame = 6
count = 3
)toml";
}

void ReplaceOnce(std::string& text, const std::string& from, const std::string& to)
{
    const std::size_t position = text.find(from);
    ASSERT_NE(position, std::string::npos);
    text.replace(position, from.size(), to);
}

[[nodiscard]] ParticleProgram ParseProgram(const std::string& text)
{
    const auto parsed = ParseParticleEffectToml(
        text,
        "effects/hit_spark.trace2d.particle.toml",
        {},
        "memory");
    EXPECT_TRUE(parsed.Succeeded());
    EXPECT_NE(parsed.asset, nullptr);
    if (!parsed.Succeeded() || parsed.asset == nullptr)
    {
        return {};
    }
    return CompileParticleProgram(*parsed.asset);
}

TEST(ParticleProgramTests, FingerprintIsStableAcrossCanonicalRoundTripAndBackendDecision)
{
    const auto parsed = ParseParticleEffectToml(
        ValidEffectToml(),
        "effects/./hit_spark.trace2d.particle.toml");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_NE(parsed.asset, nullptr);

    const ParticleProgram cpuProgram = CompileParticleProgram(*parsed.asset);
    const std::string canonical = SaveParticleEffectToml(*parsed.asset);
    const auto reparsed = ParseParticleEffectToml(canonical, parsed.asset->id);
    ASSERT_TRUE(reparsed.Succeeded());
    const ParticleProgram roundTrippedProgram = CompileParticleProgram(*reparsed.asset);

    EXPECT_EQ(cpuProgram.fingerprint, roundTrippedProgram.fingerprint);
    EXPECT_EQ(cpuProgram.selectedBackend, ParticleEffectBackend::Cpu);

    std::string gpuText = canonical;
    ReplaceOnce(gpuText, "backend = \"cpu\"", "backend = \"gpu\"");
    const auto gpuParsed = ParseParticleEffectToml(gpuText, parsed.asset->id);
    ASSERT_TRUE(gpuParsed.Succeeded());
    const ParticleProgram gpuProgram = CompileParticleProgram(*gpuParsed.asset);

    EXPECT_EQ(gpuProgram.selectedBackend, ParticleEffectBackend::Gpu);
    EXPECT_EQ(gpuProgram.fingerprint, cpuProgram.fingerprint);
    EXPECT_EQ(cpuProgram.selectedBackend, ParticleEffectBackend::Cpu);
}

TEST(ParticleProgramTests, RichProgramTracksRandomChannelsAndMinimizesGpuState)
{
    const ParticleProgram cpuProgram = ParseProgram(ValidEffectToml());

    EXPECT_EQ(cpuProgram.requiredRandomChannelCount, 13U);
    EXPECT_EQ(cpuProgram.gpuFieldCount, 9U);
    EXPECT_EQ(cpuProgram.gpuStrideBytes, 56U);
    EXPECT_EQ(cpuProgram.gpuBufferBytes, 56ULL * 64ULL);
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        cpuProgram.cpuStoredAttributeMask,
        ParticleProgramAttribute::SpawnOrdinal));
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        cpuProgram.derivedGpuAttributeMask,
        ParticleProgramAttribute::Size));
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        cpuProgram.derivedGpuAttributeMask,
        ParticleProgramAttribute::Color));

    const auto cpuArtifact = CompileParticleGpuArtifact(cpuProgram);
    EXPECT_EQ(cpuArtifact.error, ParticleGpuCompileError::BackendNotSelected);
    EXPECT_FALSE(cpuArtifact.Ok());

    std::string gpuText = ValidEffectToml();
    ReplaceOnce(gpuText, "backend = \"cpu\"", "backend = \"gpu\"");
    const auto gpuParsed = ParseParticleEffectToml(
        gpuText,
        "effects/hit_spark.trace2d.particle.toml");
    ASSERT_TRUE(gpuParsed.Succeeded());
    const ParticleProgram gpuProgram = CompileParticleProgram(*gpuParsed.asset);
    const auto firstArtifact = CompileParticleGpuArtifact(gpuProgram);
    const auto secondArtifact = CompileParticleGpuArtifact(gpuProgram);
    ASSERT_TRUE(firstArtifact.Ok());
    EXPECT_EQ(firstArtifact.artifact, secondArtifact.artifact);
    EXPECT_EQ(firstArtifact.artifact.strideBytes, 56U);
    EXPECT_EQ(firstArtifact.artifact.fieldCount, 9U);
    EXPECT_EQ(firstArtifact.artifact.fields[0].kind, ParticleGpuRuntimeFieldKind::Position);
    EXPECT_EQ(firstArtifact.artifact.fields[8].kind, ParticleGpuRuntimeFieldKind::SpriteIndex);

    ParticleEmitter2D runtimeEmitter{};
    const auto runtimePrepare = runtimeEmitter.Prepare(gpuParsed.asset, 1U, 2U);
    EXPECT_EQ(runtimePrepare.error, ParticleEmitter2DError::BackendUnavailable);
    EXPECT_FALSE(runtimeEmitter.IsPrepared());
}

TEST(ParticleProgramTests, StructuralCostMatchesCpuReferenceCountersAndMemory)
{
    const ParticleProgram program = ParseProgram(ValidEffectToml());
    ParticleEmitter2D emitter{};
    ASSERT_TRUE(PrepareParticleProgramCpuEmitter(program, 0x1234U, 77U, emitter).Ok());

    ParticleCostAccumulator accumulator{};
    accumulator.Reset(emitter);
    for (std::uint32_t frame = 0U; frame < 8U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
        accumulator.ObserveAfterStep(emitter);
    }

    const auto observation = accumulator.Observation(emitter);
    const auto report = BuildParticleStructuralCostReport(program, observation);
    const auto memory = emitter.Reference().MemoryReport();

    EXPECT_EQ(report.programFingerprint, program.fingerprint);
    EXPECT_EQ(report.bytesPerParticlePayload, 92U);
    EXPECT_EQ(report.particleStorageBytes, memory.particleStorageBytes);
    EXPECT_EQ(report.preparedCpuStateBytes, memory.preparedPayloadBytes);
    EXPECT_EQ(report.steadyStateSimulationAllocations, 0U);
    EXPECT_EQ(report.particleUpdates, observation.counters.updated);
    EXPECT_EQ(report.survivingParticleUpdates,
        observation.counters.updated - observation.counters.expired);
    EXPECT_EQ(report.spawnRandomEvaluations, observation.counters.spawned * 13ULL);

    const auto operationTotal = [&report](const ParticleProgramOperation operation) -> std::uint64_t
    {
        return report.operationTotals[static_cast<std::size_t>(operation)].evaluations;
    };
    EXPECT_EQ(operationTotal(ParticleProgramOperation::ApplyAcceleration), report.particleUpdates);
    EXPECT_EQ(operationTotal(ParticleProgramOperation::IntegratePosition), report.particleUpdates);
    EXPECT_EQ(operationTotal(ParticleProgramOperation::IntegrateRotation), report.particleUpdates);
    EXPECT_EQ(operationTotal(ParticleProgramOperation::AdvanceLifetime), report.particleUpdates);
    EXPECT_EQ(operationTotal(ParticleProgramOperation::EvaluateSizeOverLife), report.survivingParticleUpdates);
    EXPECT_EQ(operationTotal(ParticleProgramOperation::EvaluateColorOverLife), report.survivingParticleUpdates);
}

TEST(ParticleProgramTests, CostAccumulatorPreservesTotalsAcrossLoopResets)
{
    std::string text = ValidEffectToml();
    ReplaceOnce(text, "duration_frames = 12", "duration_frames = 2");
    ReplaceOnce(text, "loop = false", "loop = true");
    const ParticleProgram program = ParseProgram(text);

    ParticleEmitter2D emitter{};
    ASSERT_TRUE(PrepareParticleProgramCpuEmitter(program, 55U, 88U, emitter).Ok());
    ParticleCostAccumulator accumulator{};
    accumulator.Reset(emitter);

    for (std::uint32_t frame = 0U; frame < 6U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
        accumulator.ObserveAfterStep(emitter);
    }

    const auto observation = accumulator.Observation(emitter);
    EXPECT_EQ(observation.observedFrames, 6U);
    EXPECT_GT(observation.counters.spawned, emitter.Reference().Counters().spawned);
    EXPECT_GT(observation.counters.updated, emitter.Reference().Counters().updated);
    EXPECT_GE(observation.peakAlive, emitter.Reference().AliveCount());
}

TEST(ParticleProgramTests, ConstantEffectEliminatesUnusedGpuParticleFields)
{
    std::string text = ValidEffectToml();
    ReplaceOnce(text, "shape = \"circle\"", "shape = \"point\"");
    ReplaceOnce(text, "circle_radius = 4.0", "circle_radius = 0.0");
    ReplaceOnce(text, "frames = [2, 6]", "frames = [4, 4]");
    ReplaceOnce(text, "speed = [0.5, 3.0]", "speed = [0.0, 0.0]");
    ReplaceOnce(text, "angle_radians = [-3.0, 3.0]", "angle_radians = [0.0, 0.0]");
    ReplaceOnce(text, "acceleration = [0.0, -0.1]", "acceleration = [0.0, 0.0]");
    ReplaceOnce(text, "initial = [0.5, 2.0]", "initial = [1.0, 1.0]");
    ReplaceOnce(text, "end_multiplier = 0.25", "end_multiplier = 1.0");
    ReplaceOnce(text, "initial_radians = [-1.0, 1.0]", "initial_radians = [0.0, 0.0]");
    ReplaceOnce(text, "angular_velocity_radians_per_frame = [-0.2, 0.2]", "angular_velocity_radians_per_frame = [0.0, 0.0]");
    ReplaceOnce(text, "initial_min = [0.2, 0.3, 0.4, 0.5]", "initial_min = [1.0, 1.0, 1.0, 1.0]");
    ReplaceOnce(text, "initial_max = [1.0, 1.0, 1.0, 1.0]", "initial_max = [1.0, 1.0, 1.0, 1.0]");
    ReplaceOnce(text, "end = [0.1, 0.2, 0.3, 0.0]", "end = [1.0, 1.0, 1.0, 1.0]");
    ReplaceOnce(text,
        "sprites = [\"textures/particles/spark_a.png\", \"textures/particles/./spark_b.png\"]",
        "sprites = [\"textures/particles/spark_a.png\"]");

    const ParticleProgram program = ParseProgram(text);
    EXPECT_EQ(program.requiredRandomChannelCount, 0U);
    ASSERT_EQ(program.gpuFieldCount, 1U);
    EXPECT_EQ(program.gpuFields[0].kind, ParticleGpuRuntimeFieldKind::AgeFrames);
    EXPECT_EQ(program.gpuStrideBytes, 4U);
    EXPECT_EQ(program.gpuBufferBytes, 4ULL * 64ULL);
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        program.constantAttributeMask,
        ParticleProgramAttribute::Position));
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        program.constantAttributeMask,
        ParticleProgramAttribute::Velocity));
    EXPECT_TRUE(trace2d::particles::HasParticleProgramAttribute(
        program.constantAttributeMask,
        ParticleProgramAttribute::SpriteIndex));
}
} // namespace
