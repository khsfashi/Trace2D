#include <trace2d/assets/SpriteAssets.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace trace2d::assets
{
namespace
{
std::string MakeSpriteToml(const std::string_view borderLine = {})
{
    std::string text = R"(schema = "trace2d.sprite"
version = 1
sampling = "nearest"

[[pages]]
id = "main"
texture = "textures/panel.png"
size = [64, 64]
color_space = "srgb"
alpha_mode = "straight"

[[regions]]
id = "panel"
page = "main"
source_size = [16, 12]
trim_offset = [0, 0]
trim_size = [16, 12]
packed_rect = [4, 8, 16, 12]
pivot = [8, 6, 1]
packed_rotation = "none"
)";
    if (!borderLine.empty())
    {
        text.append(borderLine);
        text.push_back('\n');
    }
    return text;
}

bool HasDiagnosticPath(
    const SpriteAssetLoadResult& result,
    const std::string_view path)
{
    return std::any_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [path](const SpriteAssetDiagnostic& diagnostic)
        {
            return diagnostic.path == path;
        });
}

TEST(SpriteBorderAssetsTests, OmittedV1BorderDefaultsToZeroAndCanonicalSaveMakesItExplicit)
{
    const SpriteAssetLoadResult parsed = ParseSpriteAssetToml(
        MakeSpriteToml(),
        "sprites/panel.sprite.toml",
        "memory");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_EQ(parsed.asset->regions.size(), 1U);
    EXPECT_EQ(parsed.asset->regions[0].border, SpritePixelBorder{});

    const std::string saved = SaveSpriteAssetToml(*parsed.asset);
    EXPECT_NE(saved.find("border = [0, 0, 0, 0]"), std::string::npos);

    const SpriteAssetLoadResult reparsed = ParseSpriteAssetToml(
        saved,
        "sprites/panel.sprite.toml",
        "memory-roundtrip");
    ASSERT_TRUE(reparsed.Succeeded());
    EXPECT_EQ(*reparsed.asset, *parsed.asset);
}

TEST(SpriteBorderAssetsTests, ExplicitBorderRoundTripsAsExactSourcePixelMetadata)
{
    const SpriteAssetLoadResult parsed = ParseSpriteAssetToml(
        MakeSpriteToml("border = [3, 2, 5, 4]"),
        "sprites/panel.sprite.toml",
        "memory");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_EQ(parsed.asset->regions.size(), 1U);
    EXPECT_EQ(
        parsed.asset->regions[0].border,
        (SpritePixelBorder{3U, 2U, 5U, 4U}));

    const std::string saved = SaveSpriteAssetToml(*parsed.asset);
    EXPECT_NE(saved.find("border = [3, 2, 5, 4]"), std::string::npos);

    const SpriteAssetLoadResult reparsed = ParseSpriteAssetToml(
        saved,
        "sprites/panel.sprite.toml",
        "memory-roundtrip");
    ASSERT_TRUE(reparsed.Succeeded());
    EXPECT_EQ(*reparsed.asset, *parsed.asset);
}

TEST(SpriteBorderAssetsTests, RejectsOpposingBordersThatExceedSourceSize)
{
    const SpriteAssetLoadResult horizontal = ParseSpriteAssetToml(
        MakeSpriteToml("border = [9, 2, 8, 4]"),
        "sprites/panel.sprite.toml",
        "memory-horizontal");
    EXPECT_FALSE(horizontal.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(horizontal, "regions[0].border"));

    const SpriteAssetLoadResult vertical = ParseSpriteAssetToml(
        MakeSpriteToml("border = [3, 7, 5, 6]"),
        "sprites/panel.sprite.toml",
        "memory-vertical");
    EXPECT_FALSE(vertical.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(vertical, "regions[0].border"));
}

TEST(SpriteBorderAssetsTests, RejectsMalformedBorderArraysWithoutChangingV1Strictness)
{
    const SpriteAssetLoadResult wrongCount = ParseSpriteAssetToml(
        MakeSpriteToml("border = [1, 2, 3]"),
        "sprites/panel.sprite.toml",
        "memory-count");
    EXPECT_FALSE(wrongCount.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(wrongCount, "regions[0].border"));

    const SpriteAssetLoadResult negative = ParseSpriteAssetToml(
        MakeSpriteToml("border = [1, -2, 3, 4]"),
        "sprites/panel.sprite.toml",
        "memory-negative");
    EXPECT_FALSE(negative.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(negative, "regions[0].border[1]"));
}
} // namespace
} // namespace trace2d::assets
