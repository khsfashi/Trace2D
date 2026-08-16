#include <trace2d/agent/ParticleAuthoring.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace trace2d::agent
{
namespace
{
constexpr std::string_view EffectReference = "effects/authoring_spark.trace2d.particle.toml";
constexpr std::string_view EffectToml = R"toml(format_version = 1

[effect]
id = "authoring_spark"
backend = "cpu"
max_particles = 32
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
sprites = ["sprites/spark_a.sprite.toml", "sprites/spark_b.sprite.toml"]

[[bursts]]
frame = 0
count = 4

[[bursts]]
frame = 6
count = 3
)toml";

std::filesystem::path MakeTempProjectRoot()
{
    const testing::TestInfo* const info = testing::UnitTest::GetInstance()->current_test_info();
    std::string name{"trace2d_particle_authoring_tests_"};
    name += info != nullptr ? info->test_suite_name() : "unknown_suite";
    name.push_back('_');
    name += info != nullptr ? info->name() : "unknown_test";
    return std::filesystem::temp_directory_path() / name;
}

class TempParticleAuthoringProject final
{
public:
    TempParticleAuthoringProject()
        : root_{MakeTempProjectRoot()}
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
        error.clear();
        std::filesystem::create_directories(
            (root_ / std::filesystem::path{EffectReference}).parent_path(),
            error);
        ASSERT_FALSE(error);

        std::ofstream output{root_ / std::filesystem::path{EffectReference}, std::ios::binary};
        ASSERT_TRUE(output);
        output.write(EffectToml.data(), static_cast<std::streamsize>(EffectToml.size()));
        ASSERT_TRUE(output);
    }

    ~TempParticleAuthoringProject()
    {
        std::error_code error{};
        std::filesystem::remove_all(root_, error);
    }

    [[nodiscard]] const std::filesystem::path& Root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] std::string ReadEffectText() const
    {
        std::ifstream input{root_ / std::filesystem::path{EffectReference}, std::ios::binary};
        EXPECT_TRUE(input);
        return std::string{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
    }

private:
    std::filesystem::path root_{};
};

std::size_t CountOccurrences(const std::string_view text, const std::string_view needle)
{
    std::size_t count = 0U;
    std::size_t cursor = 0U;
    while ((cursor = text.find(needle, cursor)) != std::string_view::npos)
    {
        ++count;
        cursor += needle.size();
    }
    return count;
}

bool HasDiagnostic(
    const ParticleAuthoringResult& result,
    const ParticleAuthoringErrorCode code,
    const std::string_view sourceCode,
    const std::string_view path)
{
    for (const ParticleAuthoringDiagnostic& diagnostic : result.diagnostics)
    {
        if (diagnostic.code == code && diagnostic.sourceCode == sourceCode && diagnostic.path == path)
        {
            return true;
        }
    }
    return false;
}

TEST(ParticleAuthoringTests, CommitsBoundedMutationAndPreservesUnspecifiedState)
{
    TempParticleAuthoringProject project{};

    ParticleEffectMutation mutation{};
    mutation.semanticId = "authoring_spark_repaired";
    mutation.maxParticles = 48U;
    mutation.durationFrames = 20U;
    mutation.loop = true;
    mutation.emissionStartFrame = 2U;
    mutation.emissionCount = 3U;
    mutation.emissionEveryFrames = 4U;
    mutation.lifetimeFrames = particles::ParticleUIntRange{3U, 8U};

    const ParticleAuthoringResult result = MutateParticleEffectResource(
        project.Root(),
        EffectReference,
        mutation);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.validationPassed);
    EXPECT_TRUE(result.committed);
    EXPECT_EQ(
        result.changedFields,
        (std::vector<std::string>{
            "effect.id",
            "effect.max_particles",
            "effect.duration_frames",
            "effect.loop",
            "emission.start_frame",
            "emission.count",
            "emission.every_frames",
            "lifetime.frames",
        }));

    particles::ParticleEffectCache cache{project.Root()};
    const particles::ParticleEffectLoadResult reloaded = cache.Load(EffectReference);
    ASSERT_TRUE(reloaded.Succeeded());
    ASSERT_NE(reloaded.asset, nullptr);

    EXPECT_EQ(reloaded.asset->semanticId, "authoring_spark_repaired");
    EXPECT_EQ(reloaded.asset->definition.maxParticles, 48U);
    EXPECT_EQ(reloaded.asset->lifecycle.durationFrames, 20U);
    EXPECT_TRUE(reloaded.asset->lifecycle.loop);
    EXPECT_TRUE(reloaded.asset->lifecycle.playOnLoad);
    EXPECT_EQ(reloaded.asset->definition.periodicStartFrame, 2U);
    EXPECT_EQ(reloaded.asset->definition.periodicCount, 3U);
    EXPECT_EQ(reloaded.asset->definition.periodicEveryFrames, 4U);
    EXPECT_EQ(reloaded.asset->definition.lifetimeFrames, (particles::ParticleUIntRange{3U, 8U}));

    EXPECT_EQ(reloaded.asset->backend, particles::ParticleEffectBackend::Cpu);
    EXPECT_EQ(reloaded.asset->definition.simulationSpace, particles::ParticleSimulationSpace::World);
    EXPECT_EQ(reloaded.asset->definition.spawnShape.type, particles::ParticleSpawnShapeType::Circle);
    EXPECT_EQ(reloaded.asset->definition.spawnShape.circleRadius, 4.0F);
    EXPECT_EQ(reloaded.asset->definition.speed, (particles::ParticleFloatRange{0.5F, 3.0F}));
    EXPECT_EQ(reloaded.asset->blendMode, particles::ParticleBlendMode::Additive);
    ASSERT_EQ(reloaded.asset->spriteReferences.size(), 2U);
    EXPECT_EQ(reloaded.asset->spriteReferences[1], "sprites/spark_b.sprite.toml");
    ASSERT_EQ(reloaded.asset->bursts.size(), 2U);
    EXPECT_EQ(reloaded.asset->bursts[1], (particles::ParticleBurst{6U, 3U}));

    const std::string committedText = project.ReadEffectText();
    EXPECT_EQ(CountOccurrences(committedText, "max_particles ="), 1U);
    EXPECT_EQ(CountOccurrences(committedText, "duration_frames ="), 1U);
    EXPECT_EQ(CountOccurrences(committedText, "frames ="), 1U);
    const particles::ParticleEffectLoadResult reparsed = particles::ParseParticleEffectToml(
        committedText,
        EffectReference);
    EXPECT_TRUE(reparsed.Succeeded());
}

TEST(ParticleAuthoringTests, RejectsCapacityBudgetViolationWithoutChangingResource)
{
    TempParticleAuthoringProject project{};
    const std::string before = project.ReadEffectText();

    particles::ParticleReferenceLimits limits{};
    limits.maxParticlesPerEmitter = 64U;

    ParticleEffectMutation mutation{};
    mutation.maxParticles = 65U;

    const ParticleAuthoringResult result = MutateParticleEffectResource(
        project.Root(),
        EffectReference,
        mutation,
        limits);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.validationPassed);
    EXPECT_FALSE(result.committed);
    EXPECT_TRUE(HasDiagnostic(
        result,
        ParticleAuthoringErrorCode::ValidationFailed,
        "capacity_exceeds_limit",
        "effect.max_particles"));
    EXPECT_EQ(project.ReadEffectText(), before);
}

TEST(ParticleAuthoringTests, RejectsEmissionBudgetViolationWithoutChangingResource)
{
    TempParticleAuthoringProject project{};
    const std::string before = project.ReadEffectText();

    ParticleEffectMutation mutation{};
    mutation.emissionCount = 65'537U;

    const ParticleAuthoringResult result = MutateParticleEffectResource(
        project.Root(),
        EffectReference,
        mutation);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.validationPassed);
    EXPECT_FALSE(result.committed);
    EXPECT_TRUE(HasDiagnostic(
        result,
        ParticleAuthoringErrorCode::ValidationFailed,
        "capacity_exceeds_limit",
        "emission.count"));
    EXPECT_EQ(project.ReadEffectText(), before);
}

TEST(ParticleAuthoringTests, RejectsInvalidLifetimeWithoutChangingResource)
{
    TempParticleAuthoringProject project{};
    const std::string before = project.ReadEffectText();

    ParticleEffectMutation mutation{};
    mutation.lifetimeFrames = particles::ParticleUIntRange{0U, 8U};

    const ParticleAuthoringResult result = MutateParticleEffectResource(
        project.Root(),
        EffectReference,
        mutation);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.validationPassed);
    EXPECT_FALSE(result.committed);
    EXPECT_TRUE(HasDiagnostic(
        result,
        ParticleAuthoringErrorCode::ValidationFailed,
        "schema_error",
        "lifetime.frames"));
    EXPECT_EQ(project.ReadEffectText(), before);
}

TEST(ParticleAuthoringTests, EquivalentTypedMutationDoesNotRewriteResource)
{
    TempParticleAuthoringProject project{};
    const std::string before = project.ReadEffectText();

    ParticleEffectMutation mutation{};
    mutation.maxParticles = 32U;
    mutation.loop = false;
    mutation.playOnLoad = true;
    mutation.lifetimeFrames = particles::ParticleUIntRange{2U, 6U};

    const ParticleAuthoringResult result = MutateParticleEffectResource(
        project.Root(),
        EffectReference,
        mutation);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.validationPassed);
    EXPECT_FALSE(result.committed);
    EXPECT_TRUE(result.changedFields.empty());
    EXPECT_EQ(project.ReadEffectText(), before);
}
} // namespace
} // namespace trace2d::agent
