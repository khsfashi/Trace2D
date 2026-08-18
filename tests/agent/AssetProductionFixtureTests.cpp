#include <trace2d/agent/AssetProductionSpec.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

TEST(AssetProductionFixtureTests, CommittedTenAssetProofRecoversWithoutChatState)
{
    const std::string productionText =
        ReadTestData("asset_studio/forest_monsters.asset-production.trace2d.toml");
    const std::string workText =
        ReadTestData("asset_studio/forest_monsters.work.trace2d.toml");

    const auto production = trace2d::agent::ParseAssetProductionSpecToml(
        productionText,
        "forest_monsters.asset-production.trace2d.toml");
    const auto work = trace2d::agent::ParseWorkSpecToml(
        workText,
        "forest_monsters.work.trace2d.toml");

    ASSERT_TRUE(production.Succeeded());
    ASSERT_TRUE(work.Succeeded());
    ASSERT_TRUE(production.spec.has_value());
    ASSERT_TRUE(work.spec.has_value());

    EXPECT_EQ(production.spec->workSpecId, work.spec->id);
    EXPECT_EQ(production.spec->items.size(), 10U);
    EXPECT_EQ(production.spec->candidatesPerItem, 3U);
    EXPECT_EQ(production.spec->maxProviderCalls, 30U);
    EXPECT_TRUE(production.spec->ownerReviewRequired);

    for (const trace2d::agent::AssetProductionItem& item : production.spec->items)
    {
        EXPECT_EQ(item.assetClass, "sprite");
        EXPECT_EQ(item.width, 64U);
        EXPECT_EQ(item.height, 64U);
        EXPECT_EQ(item.requiredAnimations.size(), 3U);
        EXPECT_EQ(item.requiredAnimations[0], "idle");
        EXPECT_EQ(item.requiredAnimations[1], "walk");
        EXPECT_EQ(item.requiredAnimations[2], "attack");
    }

    ASSERT_EQ(production.spec->artProfiles.size(), 1U);
    EXPECT_EQ(production.spec->artProfiles[0].approvedReferences.size(), 2U);
    ASSERT_EQ(work.spec->acceptance.size(), 2U);
    EXPECT_EQ(work.spec->acceptance[1].verification, trace2d::agent::VerificationClass::Human);
}
} // namespace
