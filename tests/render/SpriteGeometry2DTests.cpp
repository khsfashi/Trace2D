#include <trace2d/render/SpriteGeometry2D.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <numbers>

namespace trace2d::render
{
namespace
{
assets::SpriteAsset MakeGeometryAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/geometry.sprite.toml";
    asset.pages = {
        assets::SpriteAtlasPage{
            "main",
            "textures/geometry.png",
            assets::SpritePixelSize{64U, 64U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "frame",
            "main",
            assets::SpritePixelSize{4U, 2U},
            assets::SpritePixelOffset{1U, 0U},
            assets::SpritePixelSize{2U, 2U},
            assets::SpritePixelRect{12U, 18U, 2U, 2U},
            assets::SpriteRationalPivot{1, 1, 1},
            assets::SpritePackedRotation::None,
        },
    };
    return asset;
}

ResolvedSpriteRegion ResolveOnlyRegion(const assets::SpriteAsset& asset)
{
    ResolvedSpriteRegion selection{};
    EXPECT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());
    return selection;
}

scene::SpritePose2D DefaultPose()
{
    return scene::SpritePose2D{};
}

void ExpectNear(const Float2 actual, const float x, const float y, const float epsilon = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, x, epsilon);
    EXPECT_NEAR(actual.y, y, epsilon);
}

TEST(SpriteGeometry2DTests, UsesUntrimmedSourceAndExactPivotAcrossYDownToYUpBoundary)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteLogicalQuad quad{};

    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, DefaultPose(), 1.0F, quad).Succeeded());

    // source_size is 4x2 and pivot is (1,1). trim_size/packed_rect are deliberately smaller.
    ExpectNear(quad.topLeft, -1.0F, 1.0F);
    ExpectNear(quad.topRight, 3.0F, 1.0F);
    ExpectNear(quad.bottomRight, 3.0F, -1.0F);
    ExpectNear(quad.bottomLeft, -1.0F, -1.0F);
}

TEST(SpriteGeometry2DTests, RationalPivotAndPixelsPerUnitRemainDerivedWithoutClamping)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.regions[0].pivot = assets::SpriteRationalPivot{1, 3, 2};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteLogicalQuad quad{};

    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, DefaultPose(), 2.0F, quad).Succeeded());
    ExpectNear(quad.topLeft, -0.25F, 0.75F);
    ExpectNear(quad.topRight, 1.75F, 0.75F);
    ExpectNear(quad.bottomRight, 1.75F, -0.25F);
    ExpectNear(quad.bottomLeft, -0.25F, -0.25F);

    asset.regions[0].pivot = assets::SpriteRationalPivot{10, -2, 1};
    const ResolvedSpriteRegion outsideSelection = ResolveOnlyRegion(asset);
    ASSERT_TRUE(BuildSpriteLogicalQuad(outsideSelection, DefaultPose(), 1.0F, quad).Succeeded());
    ExpectNear(quad.topLeft, -10.0F, -2.0F);
    ExpectNear(quad.bottomRight, -6.0F, -4.0F);
}

TEST(SpriteGeometry2DTests, AppliesCounterClockwiseRotationThenTranslation)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{10.0F, 20.0F};
    pose.transform.rotationRadians = std::numbers::pi_v<float> * 0.5F;

    SpriteLogicalQuad quad{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, pose, 1.0F, quad).Succeeded());
    ExpectNear(quad.topLeft, 9.0F, 19.0F);
    ExpectNear(quad.topRight, 9.0F, 23.0F);
    ExpectNear(quad.bottomRight, 11.0F, 23.0F);
    ExpectNear(quad.bottomLeft, 11.0F, 19.0F);
}

TEST(SpriteGeometry2DTests, NegativeScaleAndSemanticFlipAreIndependentAndComposable)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);

    SpriteLogicalQuad baseline{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, DefaultPose(), 1.0F, baseline).Succeeded());

    scene::SpritePose2D negativeX{};
    negativeX.transform.scale.x = -1.0F;
    SpriteLogicalQuad mirrored{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, negativeX, 1.0F, mirrored).Succeeded());
    ExpectNear(mirrored.topLeft, 1.0F, 1.0F);
    ExpectNear(mirrored.topRight, -3.0F, 1.0F);

    scene::SpritePose2D negativeAndFlip = negativeX;
    negativeAndFlip.flipX = true;
    SpriteLogicalQuad cancelled{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, negativeAndFlip, 1.0F, cancelled).Succeeded());
    EXPECT_EQ(cancelled, baseline);

    scene::SpritePose2D zeroScale{};
    zeroScale.transform.scale = scene::Vector2{0.0F, 0.0F};
    SpriteLogicalQuad degenerate{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, zeroScale, 1.0F, degenerate).Succeeded());
    EXPECT_EQ(degenerate.topLeft, (Float2{0.0F, 0.0F}));
    EXPECT_EQ(degenerate.topRight, degenerate.topLeft);
    EXPECT_EQ(degenerate.bottomRight, degenerate.topLeft);
    EXPECT_EQ(degenerate.bottomLeft, degenerate.topLeft);
}

TEST(SpriteGeometry2DTests, RejectsInvalidSelectionPosePixelsPerUnitAndCanonicalMathInputs)
{
    SpriteLogicalQuad quad{};
    EXPECT_EQ(
        BuildSpriteLogicalQuad(ResolvedSpriteRegion{}, DefaultPose(), 1.0F, quad),
        (SpriteGeometryStatus{SpriteGeometryError::UnresolvedSelection, SpriteGeometryField::Selection}));

    assets::SpriteAsset asset = MakeGeometryAsset();
    ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    scene::SpritePose2D invalidPose{};
    invalidPose.transform.rotationRadians = std::numeric_limits<float>::infinity();
    EXPECT_EQ(
        BuildSpriteLogicalQuad(selection, invalidPose, 1.0F, quad),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPose, SpriteGeometryField::Pose}));
    EXPECT_EQ(
        BuildSpriteLogicalQuad(selection, DefaultPose(), 0.0F, quad),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPixelsPerUnit, SpriteGeometryField::PixelsPerUnit}));
    EXPECT_EQ(
        BuildSpriteLogicalQuad(selection, DefaultPose(), std::numeric_limits<float>::quiet_NaN(), quad),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPixelsPerUnit, SpriteGeometryField::PixelsPerUnit}));

    asset = MakeGeometryAsset();
    asset.regions[0].sourceSize.width = 0U;
    selection = ResolveOnlyRegion(asset);
    EXPECT_EQ(
        BuildSpriteLogicalQuad(selection, DefaultPose(), 1.0F, quad),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidSourceSize, SpriteGeometryField::SourceSize}));

    asset = MakeGeometryAsset();
    asset.regions[0].pivot.denominator = 0;
    selection = ResolveOnlyRegion(asset);
    EXPECT_EQ(
        BuildSpriteLogicalQuad(selection, DefaultPose(), 1.0F, quad),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPivot, SpriteGeometryField::Pivot}));
}

TEST(SpriteGeometry2DTests, DetectsFiniteInputThatOverflowsFloatPresentationGeometry)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.regions[0].pivot = assets::SpriteRationalPivot{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::min(),
        1};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteLogicalQuad quad{};

    EXPECT_EQ(
        BuildSpriteLogicalQuad(
            selection,
            DefaultPose(),
            std::numeric_limits<float>::denorm_min(),
            quad),
        (SpriteGeometryStatus{SpriteGeometryError::GeometryOverflow, SpriteGeometryField::LogicalQuad}));
}

TEST(SpriteGeometry2DTests, ReusesCallerOwnedOutputAcrossSteadyStateExtraction)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const scene::SpritePose2D pose = DefaultPose();
    SpriteLogicalQuad quad{};

    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration)
    {
        ASSERT_TRUE(BuildSpriteLogicalQuad(selection, pose, 1.0F, quad).Succeeded());
    }
    ExpectNear(quad.topLeft, -1.0F, 1.0F);
}
} // namespace
} // namespace trace2d::render
