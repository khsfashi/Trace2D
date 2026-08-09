#include <trace2d/particles/ParticleEffect.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{
using trace2d::particles::LoadParticleSceneToml;
using trace2d::particles::ParseParticleEffectToml;
using trace2d::particles::ParticleEffectBackend;
using trace2d::particles::ParticleEffectCache;
using trace2d::particles::ParticleEffectErrorCode;
using trace2d::particles::ParticleEmitter2D;
using trace2d::particles::ParticleEmitter2DError;
using trace2d::particles::ParticleReferenceLimits;
using trace2d::particles::ParticleReferenceParticle;
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

[[nodiscard]] std::filesystem::path TestRoot()
{
    return std::filesystem::temp_directory_path() / "trace2d_particle_effect_asset_tests";
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(output.good());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    ASSERT_TRUE(output.good());
}

TEST(ParticleEffectTests, RichV1SchemaNormalizesAndRoundTripsDeterministically)
{
    const auto parsed = ParseParticleEffectToml(
        ValidEffectToml(),
        "effects/./hit_spark.trace2d.particle.toml",
        {},
        "memory");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_NE(parsed.asset, nullptr);

    EXPECT_EQ(parsed.asset->id, "effects/hit_spark.trace2d.particle.toml");
    EXPECT_EQ(parsed.asset->semanticId, "hit_spark");
    EXPECT_EQ(parsed.asset->backend, ParticleEffectBackend::Cpu);
    EXPECT_EQ(parsed.asset->definition.maxParticles, 64U);
    EXPECT_EQ(parsed.asset->lifecycle.durationFrames, 12U);
    EXPECT_FALSE(parsed.asset->lifecycle.loop);
    EXPECT_TRUE(parsed.asset->lifecycle.playOnLoad);
    EXPECT_EQ(parsed.asset->definition.spriteChoiceCount, 2U);
    ASSERT_EQ(parsed.asset->spriteReferences.size(), 2U);
    EXPECT_EQ(parsed.asset->spriteReferences[1], "textures/particles/spark_b.png");
    ASSERT_EQ(parsed.asset->bursts.size(), 2U);
    EXPECT_EQ(parsed.asset->bursts[1].frame, 6U);

    const std::string canonical = SaveParticleEffectToml(*parsed.asset);
    const auto reparsed = ParseParticleEffectToml(
        canonical,
        parsed.asset->id,
        {},
        "canonical");
    ASSERT_TRUE(reparsed.Succeeded());
    ASSERT_NE(reparsed.asset, nullptr);

    EXPECT_EQ(reparsed.asset->semanticId, parsed.asset->semanticId);
    EXPECT_EQ(reparsed.asset->backend, parsed.asset->backend);
    EXPECT_EQ(reparsed.asset->lifecycle, parsed.asset->lifecycle);
    EXPECT_EQ(reparsed.asset->definition, parsed.asset->definition);
    EXPECT_EQ(reparsed.asset->bursts, parsed.asset->bursts);
    EXPECT_EQ(reparsed.asset->spriteReferences, parsed.asset->spriteReferences);
    EXPECT_EQ(reparsed.asset->blendMode, parsed.asset->blendMode);
    EXPECT_EQ(SaveParticleEffectToml(*reparsed.asset), canonical);
}

TEST(ParticleEffectTests, UnknownFieldsAndInvalidRangesProduceStructuredDiagnostics)
{
    std::string unknown = ValidEffectToml();
    const std::size_t effectTable = unknown.find("[effect]");
    ASSERT_NE(effectTable, std::string::npos);
    unknown.insert(effectTable, "mystery = 1\n");

    const auto unknownResult = ParseParticleEffectToml(
        unknown,
        "effects/hit_spark.trace2d.particle.toml");
    ASSERT_FALSE(unknownResult.Succeeded());
    ASSERT_FALSE(unknownResult.diagnostics.empty());
    EXPECT_EQ(unknownResult.diagnostics.front().code, ParticleEffectErrorCode::SchemaError);
    EXPECT_EQ(unknownResult.diagnostics.front().path, "mystery");
    EXPECT_GT(unknownResult.diagnostics.front().line, 0U);

    std::string invalidRange = ValidEffectToml();
    ReplaceOnce(invalidRange, "frames = [2, 6]", "frames = [6, 2]");
    const auto rangeResult = ParseParticleEffectToml(
        invalidRange,
        "effects/hit_spark.trace2d.particle.toml");
    ASSERT_FALSE(rangeResult.Succeeded());
    bool foundLifetime = false;
    for (const auto& diagnostic : rangeResult.diagnostics)
    {
        foundLifetime = foundLifetime || diagnostic.path == "lifetime.frames";
    }
    EXPECT_TRUE(foundLifetime);
}

TEST(ParticleEffectTests, CapacityBudgetRejectsUnsafeAuthoredEffectBeforeSimulation)
{
    ParticleReferenceLimits limits{};
    limits.maxParticlesPerEmitter = 32U;

    const auto result = ParseParticleEffectToml(
        ValidEffectToml(),
        "effects/hit_spark.trace2d.particle.toml",
        limits);
    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.asset, nullptr);

    bool foundCapacity = false;
    for (const auto& diagnostic : result.diagnostics)
    {
        foundCapacity = foundCapacity ||
            (diagnostic.code == ParticleEffectErrorCode::CapacityExceedsLimit &&
             diagnostic.path == "effect.max_particles");
    }
    EXPECT_TRUE(foundCapacity);
}

TEST(ParticleEffectTests, ReservedGpuBackendNeverFallsBackToCpu)
{
    std::string text = ValidEffectToml();
    ReplaceOnce(text, "backend = \"cpu\"", "backend = \"gpu\"");

    const auto parsed = ParseParticleEffectToml(
        text,
        "effects/hit_spark.trace2d.particle.toml");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_NE(parsed.asset, nullptr);
    EXPECT_EQ(parsed.asset->backend, ParticleEffectBackend::Gpu);
    EXPECT_NE(SaveParticleEffectToml(*parsed.asset).find("backend = \"gpu\""), std::string::npos);

    ParticleEmitter2D emitter{};
    const auto prepare = emitter.Prepare(parsed.asset, 1U, 2U);
    EXPECT_EQ(prepare.error, ParticleEmitter2DError::BackendUnavailable);
    EXPECT_FALSE(emitter.IsPrepared());
}

TEST(ParticleEffectTests, CanonicalReferencesReuseOneImmutableCachedDefinition)
{
    const std::filesystem::path root = TestRoot();
    std::filesystem::remove_all(root);
    WriteTextFile(root / "effects" / "hit_spark.trace2d.particle.toml", ValidEffectToml());

    ParticleEffectCache cache{root};
    const auto first = cache.Load("effects/./hit_spark.trace2d.particle.toml");
    const auto second = cache.Load("effects\\hit_spark.trace2d.particle.toml");
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(first.asset.get(), second.asset.get());

    const auto metrics = cache.Metrics();
    EXPECT_EQ(metrics.requests, 2U);
    EXPECT_EQ(metrics.cacheMisses, 1U);
    EXPECT_EQ(metrics.cacheHits, 1U);
    EXPECT_EQ(metrics.cachedAssets, 1U);

    std::filesystem::remove_all(root);
}

TEST(ParticleEffectTests, EmittersShareEffectButOwnIndependentMutableSimulation)
{
    const auto parsed = ParseParticleEffectToml(
        ValidEffectToml(),
        "effects/hit_spark.trace2d.particle.toml");
    ASSERT_TRUE(parsed.Succeeded());

    ParticleEmitter2D left{};
    ParticleEmitter2D right{};
    ASSERT_TRUE(left.Prepare(parsed.asset, 0x1234U, 10U).Ok());
    ASSERT_TRUE(right.Prepare(parsed.asset, 0x1234U, 11U).Ok());
    EXPECT_EQ(left.Effect().get(), right.Effect().get());

    ASSERT_TRUE(left.Step());
    EXPECT_EQ(left.Reference().NextFrameIndex(), 1U);
    EXPECT_EQ(right.Reference().NextFrameIndex(), 0U);
    EXPECT_EQ(right.Reference().AliveCount(), 0U);

    ASSERT_TRUE(right.Step());
    ASSERT_TRUE(left.Step());
    ASSERT_TRUE(right.Step());
    ASSERT_GT(left.Reference().AliveCount(), 0U);
    ASSERT_GT(right.Reference().AliveCount(), 0U);

    ParticleReferenceParticle leftParticle{};
    ParticleReferenceParticle rightParticle{};
    ASSERT_TRUE(left.Reference().TryGetParticle(0U, leftParticle));
    ASSERT_TRUE(right.Reference().TryGetParticle(0U, rightParticle));
    EXPECT_NE(leftParticle.position.x, rightParticle.position.x);
    EXPECT_EQ(left.Reference().MemoryReport().steadyStateSimulationAllocations, 0U);
    EXPECT_EQ(right.Reference().MemoryReport().steadyStateSimulationAllocations, 0U);
}

TEST(ParticleEffectTests, LifecycleLoopRestartsAtDeterministicFrameBoundary)
{
    std::string text = ValidEffectToml();
    ReplaceOnce(text, "duration_frames = 12", "duration_frames = 2");
    ReplaceOnce(text, "loop = false", "loop = true");

    const auto parsed = ParseParticleEffectToml(
        text,
        "effects/loop.trace2d.particle.toml");
    ASSERT_TRUE(parsed.Succeeded());

    ParticleEmitter2D emitter{};
    ASSERT_TRUE(emitter.Prepare(parsed.asset, 77U, 99U).Ok());
    ASSERT_TRUE(emitter.Step());
    EXPECT_EQ(emitter.CycleFrame(), 1U);
    ASSERT_TRUE(emitter.Step());
    EXPECT_EQ(emitter.CycleFrame(), 2U);
    EXPECT_EQ(emitter.CompletedLoops(), 1U);

    ASSERT_TRUE(emitter.Step());
    EXPECT_EQ(emitter.CycleFrame(), 1U);
    EXPECT_EQ(emitter.Reference().NextFrameIndex(), 1U);
    EXPECT_EQ(emitter.CompletedLoops(), 1U);
}

TEST(ParticleEffectTests, ParticleSceneReferencesLoadAndHeadlessSimulationNeedsNoRenderer)
{
    const std::filesystem::path root = TestRoot();
    std::filesystem::remove_all(root);
    WriteTextFile(root / "effects" / "hit_spark.trace2d.particle.toml", ValidEffectToml());

    const std::string sceneText = R"toml(format_version = 1

[scene]
id = "particle_test"
name = "Particle Test"

[[entities]]
id = "fx_anchor"
name = "FX Anchor"
tags = ["fx"]

[entities.transform]
position = [0.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]

[[particle_emitters]]
entity = "fx_anchor"
effect = "effects/./hit_spark.trace2d.particle.toml"
stable_id = 77
)toml";

    const auto loadedScene = LoadParticleSceneToml(sceneText, "particle_test.trace2d.scene.toml");
    ASSERT_TRUE(loadedScene.Succeeded());
    ASSERT_TRUE(loadedScene.scene.has_value());
    ASSERT_EQ(loadedScene.emitters.size(), 1U);
    EXPECT_EQ(loadedScene.emitters[0].entityId, "fx_anchor");
    EXPECT_EQ(loadedScene.emitters[0].effectReference, "effects/hit_spark.trace2d.particle.toml");
    EXPECT_EQ(loadedScene.emitters[0].stableId, 77U);

    ParticleEffectCache cache{root};
    const auto effect = cache.Load(loadedScene.emitters[0].effectReference);
    ASSERT_TRUE(effect.Succeeded());

    ParticleEmitter2D emitter{};
    ASSERT_TRUE(emitter.Prepare(effect.asset, 0xCAFEU, loadedScene.emitters[0].stableId).Ok());
    for (std::uint32_t frame = 0; frame < 4U; ++frame)
    {
        ASSERT_TRUE(emitter.Step());
    }
    EXPECT_GT(emitter.Reference().Counters().spawned, 0U);

    std::filesystem::remove_all(root);
}
} // namespace
