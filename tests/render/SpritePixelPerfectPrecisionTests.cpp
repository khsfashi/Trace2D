#include <trace2d/render/SpritePixelPerfect2D.hpp>

#include <gtest/gtest.h>

#include <numbers>

namespace trace2d::render
{
namespace
{
constexpr std::uint32_t LogicalWidth = 320U;
constexpr std::uint32_t LogicalHeight = 180U;
constexpr float PixelsPerUnit = 16.0F;

assets::SpriteAsset MakePrecisionAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/pixel-perfect-precision.sprite.toml";
    asset.pages = {
        assets::SpriteAtlasPage{
            "main",
            "textures/pixel-perfect-precision.png",
            assets::SpritePixelSize{4U, 4U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "frame",
            "main",
            assets::SpritePixelSize{2U, 2U},
            assets::SpritePixelOffset{0U, 0U},
            assets::SpritePixelSize{2U, 2U},
            assets::SpritePixelRect{0U, 0U, 2U, 2U},
            assets::SpriteRationalPivot{0, 0, 1},
            assets::SpritePackedRotation::None,
        },
    };
    return asset;
}

ResolvedSpriteRegion ResolvePrecisionRegion(const assets::SpriteAsset& asset)
{
    ResolvedSpriteRegion selection{};
    EXPECT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());
    return selection;
}

SpritePixelPerfectViewport2D BuildPrecisionViewport()
{
    OrthographicCamera camera{};
    camera.verticalSize = static_cast<float>(LogicalHeight) / PixelsPerUnit;
    SpritePixelPerfectViewport2D viewport{};
    EXPECT_TRUE(BuildSpritePixelPerfectViewport(
        camera,
        LogicalWidth,
        LogicalHeight,
        1280U,
        720U,
        viewport).Succeeded());
    return viewport;
}

TEST(SpritePixelPerfectPrecisionTests, LargeFractionalBasisCannotHideInsideRelativeTolerance)
{
    const assets::SpriteAsset asset = MakePrecisionAsset();
    const ResolvedSpriteRegion selection = ResolvePrecisionRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildPrecisionViewport();

    scene::SpritePose2D pose{};
    pose.transform.scale = scene::Vector2{10000.25F, 10000.25F};
    const scene::SpritePoseHistory2D history{pose, pose};
    SpritePixelPerfectMapping2D mapping{};

    EXPECT_EQ(
        ResolveSpritePixelPerfectPose(selection, history, PixelsPerUnit, viewport, {}, mapping),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis}));
    EXPECT_EQ(mapping, SpritePixelPerfectMapping2D{});
}

TEST(SpritePixelPerfectPrecisionTests, LargeIntegerQuarterTurnKeepsOnlyNumericalAxisNoise)
{
    const assets::SpriteAsset asset = MakePrecisionAsset();
    const ResolvedSpriteRegion selection = ResolvePrecisionRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildPrecisionViewport();

    scene::SpritePose2D pose{};
    pose.transform.scale = scene::Vector2{10000.0F, 10000.0F};
    pose.transform.rotationRadians = std::numbers::pi_v<float> * 0.5F;
    const scene::SpritePoseHistory2D history{pose, pose};
    SpritePixelPerfectMapping2D mapping{};

    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, history, PixelsPerUnit, viewport, {}, mapping).Succeeded());
    EXPECT_TRUE(mapping.axesSwapped);
    EXPECT_EQ(mapping.sourcePixelScaleX, 10000U);
    EXPECT_EQ(mapping.sourcePixelScaleY, 10000U);
}
} // namespace
} // namespace trace2d::render
