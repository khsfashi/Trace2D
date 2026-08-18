#include <trace2d/agent/AssetProductionSpec.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
constexpr std::string_view RepresentativeProductionSpec = R"toml(
format_version = 1

[production_set]
id = "forest-monsters-v1"
work_spec = "forest-monsters-work"
owner_review_acceptance = "owner-approval"
art_profile = "forest-monsters"
candidates_per_item = 3
max_provider_calls = 30

[[items]]
id = "mossling"
asset_class = "sprite"
width = 64
height = 64
required_animations = ["idle", "walk", "attack"]
required_directions = ["right"]
constraints = ["transparent background", "single readable silhouette"]

[[items]]
id = "thornboar"
asset_class = "sprite"
width = 64
height = 64
required_animations = ["idle", "walk", "attack"]
required_directions = ["right"]
constraints = ["transparent background", "single readable silhouette"]

[[art_profiles]]
id = "forest-monsters"
description = "Approved forest-monster reference set."
creative_constraints = [
    "pixel-art clusters remain readable at native resolution",
    "single upper-left lighting direction",
]
approved_references = [
    "assets/monsters/approved/moss_golem.sprite.toml",
    "assets/monsters/approved/thorn_wolf.sprite.toml",
]
)toml";

constexpr std::string_view RepresentativeWorkSpec = R"toml(
format_version = 1
[work]
id = "forest-monsters-work"
intent = "Produce a coherent forest-monster Sprite set."
state = "planned"
constraints = ["Owner approval remains required."]

[[deliverables]]
id = "asset-set"
description = "Forest-monster asset set"
state = "planned"

[[acceptance]]
id = "structural-proof"
deliverable = "asset-set"
description = "Canonical import verifies."
verification = "deterministic"
state = "planned"

[[acceptance]]
id = "owner-approval"
deliverable = "asset-set"
description = "Owner approves the visual set."
verification = "human"
state = "planned"
)toml";

TEST(AssetProductionSpecTests, RecoversSetIntentAndReferencesWorkSpecHumanApproval)
{
    const auto production = trace2d::agent::ParseAssetProductionSpecToml(
        RepresentativeProductionSpec,
        "forest.asset-production.toml");
    const auto work = trace2d::agent::ParseWorkSpecToml(
        RepresentativeWorkSpec,
        "forest.work.toml");

    ASSERT_TRUE(production.Succeeded());
    ASSERT_TRUE(work.Succeeded());
    ASSERT_TRUE(production.spec.has_value());
    ASSERT_TRUE(work.spec.has_value());

    EXPECT_EQ(production.spec->id, "forest-monsters-v1");
    EXPECT_EQ(production.spec->workSpecId, work.spec->id);
    EXPECT_EQ(production.spec->ownerReviewAcceptanceId, "owner-approval");
    EXPECT_EQ(production.spec->artProfileId, "forest-monsters");
    EXPECT_EQ(production.spec->candidatesPerItem, 3U);
    EXPECT_EQ(production.spec->maxProviderCalls, 30U);
    ASSERT_EQ(production.spec->items.size(), 2U);
    EXPECT_EQ(production.spec->items[0].width, 64U);
    EXPECT_EQ(production.spec->items[0].height, 64U);
    ASSERT_EQ(production.spec->items[0].requiredAnimations.size(), 3U);
    EXPECT_EQ(production.spec->items[0].requiredAnimations[2], "attack");
    ASSERT_EQ(production.spec->artProfiles.size(), 1U);
    ASSERT_EQ(production.spec->artProfiles[0].approvedReferences.size(), 2U);

    const auto linkDiagnostics = trace2d::agent::ValidateAssetProductionSpecAgainstWorkSpec(
        *production.spec,
        *work.spec);
    EXPECT_TRUE(linkDiagnostics.empty());
}

TEST(AssetProductionSpecTests, RejectsProviderSpecificConfiguration)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[production_set]
id = "provider-coupled"
work_spec = "work"
owner_review_acceptance = "approval"
art_profile = "profile"
candidates_per_item = 2
max_provider_calls = 4
provider_model = "vendor/model"

[[items]]
id = "item"
asset_class = "sprite"
width = 64
height = 64

[[art_profiles]]
id = "profile"
description = "Reference profile"
approved_references = ["assets/reference.sprite.toml"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics[0].path, "production_set.provider_model");
}

TEST(AssetProductionSpecTests, RejectsAestheticScoreAsDeterministicProfileTruth)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[production_set]
id = "scored-style"
work_spec = "work"
owner_review_acceptance = "approval"
art_profile = "profile"
candidates_per_item = 2
max_provider_calls = 4

[[items]]
id = "item"
asset_class = "sprite"
width = 64
height = 64

[[art_profiles]]
id = "profile"
description = "Reference profile"
aesthetic_score = 0.9
approved_references = ["assets/reference.sprite.toml"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics[0].path, "art_profiles[0].aesthetic_score");
}

TEST(AssetProductionSpecTests, RejectsUnknownArtProfileReference)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[production_set]
id = "missing-profile"
work_spec = "work"
owner_review_acceptance = "approval"
art_profile = "missing"
candidates_per_item = 1
max_provider_calls = 1

[[items]]
id = "item"
asset_class = "sprite"
width = 64
height = 64

[[art_profiles]]
id = "known"
description = "Known profile"
approved_references = ["assets/reference.sprite.toml"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics.back().path, "production_set.art_profile");
}

TEST(AssetProductionSpecTests, RejectsNonProjectRelativeApprovedReferences)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[production_set]
id = "unsafe-reference"
work_spec = "work"
owner_review_acceptance = "approval"
art_profile = "profile"
candidates_per_item = 1
max_provider_calls = 1

[[items]]
id = "item"
asset_class = "sprite"
width = 64
height = 64

[[art_profiles]]
id = "profile"
description = "Reference profile"
approved_references = ["../outside.sprite.toml"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    ASSERT_FALSE(parsed.diagnostics.empty());
    EXPECT_EQ(parsed.diagnostics.back().path, "art_profiles[0].approved_references[0]");
}

TEST(AssetProductionSpecTests, RejectsUnboundedZeroBudgets)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[production_set]
id = "unbounded"
work_spec = "work"
owner_review_acceptance = "approval"
art_profile = "profile"
candidates_per_item = 0
max_provider_calls = 0

[[items]]
id = "item"
asset_class = "sprite"
width = 64
height = 64

[[art_profiles]]
id = "profile"
description = "Reference profile"
approved_references = ["assets/reference.sprite.toml"]
)toml";

    const auto parsed = trace2d::agent::ParseAssetProductionSpecToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    ASSERT_GE(parsed.diagnostics.size(), 2U);
}

TEST(AssetProductionSpecTests, CrossValidationRejectsUnknownReviewAcceptance)
{
    const auto production = trace2d::agent::ParseAssetProductionSpecToml(RepresentativeProductionSpec);
    const auto work = trace2d::agent::ParseWorkSpecToml(RepresentativeWorkSpec);
    ASSERT_TRUE(production.Succeeded());
    ASSERT_TRUE(work.Succeeded());
    ASSERT_TRUE(production.spec.has_value());
    ASSERT_TRUE(work.spec.has_value());

    auto modified = *production.spec;
    modified.ownerReviewAcceptanceId = "missing-approval";
    const auto diagnostics = trace2d::agent::ValidateAssetProductionSpecAgainstWorkSpec(
        modified,
        *work.spec);

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics[0].path, "production_set.owner_review_acceptance");
}

TEST(AssetProductionSpecTests, CrossValidationRejectsDeterministicReviewAcceptance)
{
    const auto production = trace2d::agent::ParseAssetProductionSpecToml(RepresentativeProductionSpec);
    const auto work = trace2d::agent::ParseWorkSpecToml(RepresentativeWorkSpec);
    ASSERT_TRUE(production.Succeeded());
    ASSERT_TRUE(work.Succeeded());
    ASSERT_TRUE(production.spec.has_value());
    ASSERT_TRUE(work.spec.has_value());

    auto modified = *production.spec;
    modified.ownerReviewAcceptanceId = "structural-proof";
    const auto diagnostics = trace2d::agent::ValidateAssetProductionSpecAgainstWorkSpec(
        modified,
        *work.spec);

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics[0].path, "production_set.owner_review_acceptance");
}
} // namespace
