#include <trace2d/agent/AssetCandidateSet.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
std::string ReadTestData(const std::filesystem::path& relativePath)
{
    const std::filesystem::path path = std::filesystem::path{TRACE2D_TEST_DATA_DIR} / relativePath;
    std::ifstream input{path, std::ios::binary};
    EXPECT_TRUE(input) << path.string();
    std::ostringstream contents{};
    contents << input.rdbuf();
    return contents.str();
}

trace2d::agent::AssetProductionSpec ParseProductionFixture()
{
    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(
        ReadTestData("asset_studio/forest_monsters.asset-production.trace2d.toml"),
        "forest_monsters.asset-production.trace2d.toml");
    EXPECT_TRUE(parsed.Succeeded());
    return *parsed.spec;
}

trace2d::agent::WorkspaceSnapshot BuildWorkspaceFixture()
{
    const auto work = trace2d::agent::ParseWorkSpecToml(
        ReadTestData("asset_studio/forest_monsters.work.trace2d.toml"),
        "forest_monsters.work.trace2d.toml");
    const auto result = trace2d::agent::ParseWorkResultToml(
        ReadTestData("asset_studio/forest_monsters.candidate-review.work-result.trace2d.toml"),
        "forest_monsters.candidate-review.work-result.trace2d.toml");
    EXPECT_TRUE(work.Succeeded());
    EXPECT_TRUE(result.Succeeded());
    return trace2d::agent::BuildWorkspaceSnapshot(*work.spec, *result.result);
}

TEST(AssetCandidateSetTests, CommittedCandidatesComposeOnlyCurrentWorkspaceArtifacts)
{
    const auto parsed = trace2d::agent::ParseAssetCandidateSetToml(
        ReadTestData("asset_studio/mossling.candidate-set.trace2d.toml"),
        "mossling.candidate-set.trace2d.toml");
    ASSERT_TRUE(parsed.Succeeded());

    const auto production = ParseProductionFixture();
    const auto workspace = BuildWorkspaceFixture();
    ASSERT_EQ(workspace.currentRevisionId, "candidate-review-r1");
    ASSERT_EQ(workspace.currentArtifacts.size(), 6U);
    ASSERT_EQ(workspace.reviewQueue.size(), 1U);

    const auto comparison = trace2d::agent::BuildAssetCandidateComparison(
        *parsed.candidateSet,
        production,
        workspace);
    ASSERT_TRUE(comparison.Succeeded());
    ASSERT_TRUE(comparison.comparison.has_value());
    EXPECT_EQ(comparison.comparison->productionSetId, "forest-monsters-v1");
    EXPECT_EQ(comparison.comparison->itemId, "mossling");
    EXPECT_EQ(comparison.comparison->revisionId, "candidate-review-r1");
    ASSERT_EQ(comparison.comparison->candidates.size(), 3U);

    for (std::size_t index = 0; index < comparison.comparison->candidates.size(); ++index)
    {
        const auto& candidate = comparison.comparison->candidates[index];
        EXPECT_EQ(candidate.ordinal, static_cast<std::uint32_t>(index + 1U));
        ASSERT_EQ(candidate.artifacts.size(), 2U);
        EXPECT_EQ(candidate.artifacts[0].kind, "image");
        EXPECT_EQ(candidate.artifacts[1].kind, "image");
    }
}

TEST(AssetCandidateSetTests, RejectsParallelLifecycleProviderAndAestheticFields)
{
    constexpr std::string_view text = R"toml(
format_version = 1

[candidate_set]
production_set = "forest-monsters-v1"
item = "mossling"
work_id = "asset-studio-forest-monsters"
revision = "r1"
provider = "some-provider"

[[candidates]]
id = "a"
ordinal = 1
artifacts = ["a-native"]
status = "approved"
aesthetic_score = 0.95
)toml";

    const auto parsed = trace2d::agent::ParseAssetCandidateSetToml(text, "forbidden-fields.toml");
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_GE(parsed.diagnostics.size(), 3U);
}

TEST(AssetCandidateSetTests, RejectsDuplicateCandidateIdentityAndOrdinal)
{
    constexpr std::string_view text = R"toml(
format_version = 1

[candidate_set]
production_set = "forest-monsters-v1"
item = "mossling"
work_id = "asset-studio-forest-monsters"
revision = "r1"

[[candidates]]
id = "same"
ordinal = 1
artifacts = ["a"]

[[candidates]]
id = "same"
ordinal = 1
artifacts = ["b"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetCandidateSetToml(text, "duplicates.toml");
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_GE(parsed.diagnostics.size(), 2U);
}

TEST(AssetCandidateSetTests, EnforcesProductionItemAndCandidateBudget)
{
    auto production = ParseProductionFixture();
    trace2d::agent::AssetCandidateSet candidateSet{};
    candidateSet.productionSetId = production.id;
    candidateSet.itemId = "not-a-production-item";
    candidateSet.workId = production.workSpecId;
    candidateSet.revisionId = "r1";

    for (std::uint32_t ordinal = 1; ordinal <= 4; ++ordinal)
    {
        trace2d::agent::AssetCandidate candidate{};
        candidate.id = "candidate-" + std::to_string(ordinal);
        candidate.ordinal = ordinal;
        candidate.artifactIds = {"artifact-" + std::to_string(ordinal)};
        candidateSet.candidates.push_back(std::move(candidate));
    }

    const auto diagnostics = trace2d::agent::ValidateAssetCandidateSetAgainstProductionSpec(
        candidateSet,
        production);
    EXPECT_GE(diagnostics.size(), 3U);
}

TEST(AssetCandidateSetTests, RejectsStaleRevisionAndUnknownArtifact)
{
    const auto parsed = trace2d::agent::ParseAssetCandidateSetToml(
        ReadTestData("asset_studio/mossling.candidate-set.trace2d.toml"));
    ASSERT_TRUE(parsed.Succeeded());

    auto candidateSet = *parsed.candidateSet;
    candidateSet.revisionId = "candidate-review-r0";
    candidateSet.candidates[0].artifactIds[0] = "not-owned-by-current-revision";

    const auto comparison = trace2d::agent::BuildAssetCandidateComparison(
        candidateSet,
        ParseProductionFixture(),
        BuildWorkspaceFixture());
    EXPECT_FALSE(comparison.Succeeded());
    EXPECT_GE(comparison.diagnostics.size(), 2U);
}
} // namespace
