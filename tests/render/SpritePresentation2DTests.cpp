#include <trace2d/render/SpritePresentation2D.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace trace2d::render
{
namespace
{
assets::SpriteAsset MakeAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/presentation.sprite.toml";
    asset.sampling = assets::SpriteSampling::Linear;
    asset.pages = {
        assets::SpriteAtlasPage{
            "page",
            "textures/presentation.png",
            assets::SpritePixelSize{32U, 32U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "region",
            "page",
            assets::SpritePixelSize{8U, 8U},
            assets::SpritePixelOffset{2U, 1U},
            assets::SpritePixelSize{4U, 6U},
            assets::SpritePixelRect{8U, 10U, 4U, 6U},
            assets::SpriteRationalPivot{4, 4, 1},
            assets::SpritePackedRotation::None,
        },
    };
    return asset;
}

ResolvedSpriteRegion Resolve(const assets::SpriteAsset& asset)
{
    ResolvedSpriteRegion selection{};
    EXPECT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());
    return selection;
}

TEST(SpritePresentation2DTests, CombinesExactSr2GeometryAndSr3Appearance)
{
    const assets::SpriteAsset asset = MakeAsset();
    const ResolvedSpriteRegion selection = Resolve(asset);
    const scene::SpritePose2D pose{};
    SpriteAppearance2D appearance{};
    appearance.tint = SpriteLinearRgba{0.5F, 0.75F, 1.0F, 0.8F};
    appearance.opacity = 0.5F;
    appearance.blend = SpriteBlendMode::Screen;

    SpriteDrawQuad expectedQuad{};
    ASSERT_TRUE(BuildSpriteDrawQuad(selection, pose, 2.0F, expectedQuad).Succeeded());
    SpriteAppearanceContractData expectedAppearance{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, appearance, expectedAppearance).Succeeded());

    SpritePresentation2D presentation{};
    const SpritePresentationStatus status =
        BuildSpritePresentation2D(selection, pose, 2.0F, appearance, presentation);

    ASSERT_TRUE(status.Succeeded());
    EXPECT_EQ(presentation.quad, expectedQuad);
    EXPECT_EQ(presentation.appearance, expectedAppearance);
    EXPECT_EQ(presentation.appearance.sampler, SpriteSamplerCompatibility::Linear);
    EXPECT_EQ(presentation.appearance.blend, SpriteBlendCompatibility::Screen);
}

TEST(SpritePresentation2DTests, GeometryFailureDoesNotRunPartialPresentation)
{
    const assets::SpriteAsset asset = MakeAsset();
    const ResolvedSpriteRegion selection = Resolve(asset);
    SpritePresentation2D presentation{};
    presentation.appearance.opacity = 0.25F;

    const SpritePresentationStatus status = BuildSpritePresentation2D(
        selection,
        scene::SpritePose2D{},
        0.0F,
        SpriteAppearance2D{},
        presentation);

    EXPECT_EQ(status.error, SpritePresentationError::Geometry);
    EXPECT_FALSE(status.geometry.Succeeded());
    EXPECT_TRUE(status.appearance.Succeeded());
    EXPECT_EQ(presentation, SpritePresentation2D{});
}

TEST(SpritePresentation2DTests, AppearanceFailureDoesNotLeakValidGeometry)
{
    const assets::SpriteAsset asset = MakeAsset();
    const ResolvedSpriteRegion selection = Resolve(asset);
    SpriteAppearance2D appearance{};
    appearance.opacity = std::numeric_limits<float>::quiet_NaN();

    SpritePresentation2D presentation{};
    const SpritePresentationStatus status = BuildSpritePresentation2D(
        selection,
        scene::SpritePose2D{},
        1.0F,
        appearance,
        presentation);

    EXPECT_EQ(status.error, SpritePresentationError::Appearance);
    EXPECT_TRUE(status.geometry.Succeeded());
    EXPECT_EQ(
        status.appearance,
        (SpriteAppearanceStatus{
            SpriteAppearanceError::InvalidOpacity,
            SpriteAppearanceField::Opacity}));
    EXPECT_EQ(presentation, SpritePresentation2D{});
}
} // namespace
} // namespace trace2d::render
