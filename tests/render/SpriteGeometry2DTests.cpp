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

TEST(SpriteGeometry2DTests, FullUntrimmedDrawMatchesLogicalQuadExactly)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.regions[0].trimOffset = assets::SpritePixelOffset{0U, 0U};
    asset.regions[0].trimSize = asset.regions[0].sourceSize;
    asset.regions[0].packedRect = assets::SpritePixelRect{8U, 16U, 4U, 2U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);

    SpriteLogicalQuad logical{};
    SpriteDrawQuad draw{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(selection, DefaultPose(), 1.0F, logical).Succeeded());
    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw).Succeeded());

    EXPECT_EQ(draw.topLeft.position, logical.topLeft);
    EXPECT_EQ(draw.topRight.position, logical.topRight);
    EXPECT_EQ(draw.bottomRight.position, logical.bottomRight);
    EXPECT_EQ(draw.bottomLeft.position, logical.bottomLeft);
}

TEST(SpriteGeometry2DTests, TrimmedDrawUsesExactTrimRectangleInUntrimmedSourceSpace)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteDrawQuad draw{};

    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw).Succeeded());

    // trim [1,0,2,2] stays embedded in source [0,0,4,2] around pivot (1,1).
    ExpectNear(draw.topLeft.position, 0.0F, 1.0F);
    ExpectNear(draw.topRight.position, 2.0F, 1.0F);
    ExpectNear(draw.bottomRight.position, 2.0F, -1.0F);
    ExpectNear(draw.bottomLeft.position, 0.0F, -1.0F);
}

TEST(SpriteGeometry2DTests, TrimmedDrawPreservesTransformScaleFlipRotationAndTranslationOrder)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{10.0F, 20.0F};
    pose.transform.rotationRadians = std::numbers::pi_v<float> * 0.5F;
    pose.transform.scale = scene::Vector2{2.0F, -1.0F};
    pose.flipX = true;

    SpriteDrawQuad draw{};
    ASSERT_TRUE(BuildSpriteDrawQuad(selection, pose, 2.0F, draw).Succeeded());

    ExpectNear(draw.topLeft.position, 10.5F, 20.0F);
    ExpectNear(draw.topRight.position, 10.5F, 18.0F);
    ExpectNear(draw.bottomRight.position, 9.5F, 18.0F);
    ExpectNear(draw.bottomLeft.position, 9.5F, 20.0F);
}

TEST(SpriteGeometry2DTests, DrawPreservesExactRationalAndOutOfSourcePivotWithoutClamping)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.regions[0].pivot = assets::SpriteRationalPivot{1, 3, 2};
    ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteDrawQuad draw{};

    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 2.0F, draw).Succeeded());
    ExpectNear(draw.topLeft.position, 0.25F, 0.75F);
    ExpectNear(draw.bottomRight.position, 1.25F, -0.25F);

    asset.regions[0].pivot = assets::SpriteRationalPivot{10, -2, 1};
    selection = ResolveOnlyRegion(asset);
    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw).Succeeded());
    ExpectNear(draw.topLeft.position, -9.0F, -2.0F);
    ExpectNear(draw.bottomRight.position, -7.0F, -4.0F);
}

TEST(SpriteGeometry2DTests, NoneRotationUsesCanonicalTopLeftPixelEdgeUvsWithoutHalfTexelOffset)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.pages[0].size = assets::SpritePixelSize{100U, 80U};
    asset.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 2U, 2U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteDrawQuad draw{};

    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw).Succeeded());

    ExpectNear(draw.topLeft.uv, 0.10F, 0.25F);
    ExpectNear(draw.topRight.uv, 0.12F, 0.25F);
    ExpectNear(draw.bottomRight.uv, 0.12F, 0.275F);
    ExpectNear(draw.bottomLeft.uv, 0.10F, 0.275F);
}

TEST(SpriteGeometry2DTests, Cw90StorageOnlyPermutesUvsAndNeverChangesLogicalPlacement)
{
    assets::SpriteAsset unrotated = MakeGeometryAsset();
    unrotated.regions[0].sourceSize = assets::SpritePixelSize{5U, 6U};
    unrotated.regions[0].trimOffset = assets::SpritePixelOffset{1U, 2U};
    unrotated.regions[0].trimSize = assets::SpritePixelSize{2U, 3U};
    unrotated.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 2U, 3U};
    unrotated.regions[0].pivot = assets::SpriteRationalPivot{2, 4, 1};

    assets::SpriteAsset rotated = unrotated;
    rotated.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 3U, 2U};
    rotated.regions[0].packedRotation = assets::SpritePackedRotation::Cw90;

    const ResolvedSpriteRegion unrotatedSelection = ResolveOnlyRegion(unrotated);
    const ResolvedSpriteRegion rotatedSelection = ResolveOnlyRegion(rotated);
    SpriteDrawQuad noneDraw{};
    SpriteDrawQuad cwDraw{};
    ASSERT_TRUE(BuildSpriteDrawQuad(unrotatedSelection, DefaultPose(), 1.0F, noneDraw).Succeeded());
    ASSERT_TRUE(BuildSpriteDrawQuad(rotatedSelection, DefaultPose(), 1.0F, cwDraw).Succeeded());

    EXPECT_EQ(noneDraw.topLeft.position, cwDraw.topLeft.position);
    EXPECT_EQ(noneDraw.topRight.position, cwDraw.topRight.position);
    EXPECT_EQ(noneDraw.bottomRight.position, cwDraw.bottomRight.position);
    EXPECT_EQ(noneDraw.bottomLeft.position, cwDraw.bottomLeft.position);

    // cw90 logical TL->packed TR, TR->BR, BR->BL, BL->TL.
    ExpectNear(cwDraw.topLeft.uv, 13.0F / 64.0F, 20.0F / 64.0F);
    ExpectNear(cwDraw.topRight.uv, 13.0F / 64.0F, 22.0F / 64.0F);
    ExpectNear(cwDraw.bottomRight.uv, 10.0F / 64.0F, 22.0F / 64.0F);
    ExpectNear(cwDraw.bottomLeft.uv, 10.0F / 64.0F, 20.0F / 64.0F);
}

TEST(SpriteGeometry2DTests, PackedRectAtPageBoundaryMapsExactlyToZeroAndOneEdges)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    asset.pages[0].size = assets::SpritePixelSize{8U, 4U};
    asset.regions[0].sourceSize = assets::SpritePixelSize{8U, 4U};
    asset.regions[0].trimOffset = assets::SpritePixelOffset{0U, 0U};
    asset.regions[0].trimSize = assets::SpritePixelSize{8U, 4U};
    asset.regions[0].packedRect = assets::SpritePixelRect{0U, 0U, 8U, 4U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteDrawQuad draw{};

    ASSERT_TRUE(BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw).Succeeded());
    EXPECT_EQ(draw.topLeft.uv, (Float2{0.0F, 0.0F}));
    EXPECT_EQ(draw.topRight.uv, (Float2{1.0F, 0.0F}));
    EXPECT_EQ(draw.bottomRight.uv, (Float2{1.0F, 1.0F}));
    EXPECT_EQ(draw.bottomLeft.uv, (Float2{0.0F, 1.0F}));
}

TEST(SpriteGeometry2DTests, RejectsCorruptedPageTrimAndPackedBoundsAtDrawBoundary)
{
    SpriteDrawQuad draw{};

    assets::SpriteAsset pageAsset = MakeGeometryAsset();
    const ResolvedSpriteRegion pageSelection = ResolveOnlyRegion(pageAsset);
    pageAsset.pages[0].size.width = 0U;
    EXPECT_EQ(
        BuildSpriteDrawQuad(pageSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPageSize, SpriteGeometryField::PageSize}));

    assets::SpriteAsset zeroTrimAsset = MakeGeometryAsset();
    const ResolvedSpriteRegion zeroTrimSelection = ResolveOnlyRegion(zeroTrimAsset);
    zeroTrimAsset.regions[0].trimSize.width = 0U;
    EXPECT_EQ(
        BuildSpriteDrawQuad(zeroTrimSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidTrimRect, SpriteGeometryField::TrimRect}));

    assets::SpriteAsset outsideTrimAsset = MakeGeometryAsset();
    const ResolvedSpriteRegion outsideTrimSelection = ResolveOnlyRegion(outsideTrimAsset);
    outsideTrimAsset.regions[0].trimOffset.x = 4U;
    EXPECT_EQ(
        BuildSpriteDrawQuad(outsideTrimSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidTrimRect, SpriteGeometryField::TrimRect}));

    assets::SpriteAsset zeroPackedAsset = MakeGeometryAsset();
    const ResolvedSpriteRegion zeroPackedSelection = ResolveOnlyRegion(zeroPackedAsset);
    zeroPackedAsset.regions[0].packedRect.height = 0U;
    EXPECT_EQ(
        BuildSpriteDrawQuad(zeroPackedSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPackedRect, SpriteGeometryField::PackedRect}));

    assets::SpriteAsset outsidePackedAsset = MakeGeometryAsset();
    const ResolvedSpriteRegion outsidePackedSelection = ResolveOnlyRegion(outsidePackedAsset);
    outsidePackedAsset.regions[0].packedRect.x = 63U;
    EXPECT_EQ(
        BuildSpriteDrawQuad(outsidePackedSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPackedRect, SpriteGeometryField::PackedRect}));
}

TEST(SpriteGeometry2DTests, RejectsPackedExtentMismatchAndUnsupportedRotation)
{
    SpriteDrawQuad draw{};

    assets::SpriteAsset noneMismatch = MakeGeometryAsset();
    noneMismatch.regions[0].packedRect.width = 3U;
    const ResolvedSpriteRegion noneSelection = ResolveOnlyRegion(noneMismatch);
    EXPECT_EQ(
        BuildSpriteDrawQuad(noneSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::PackedExtentMismatch, SpriteGeometryField::PackedRect}));

    assets::SpriteAsset cwMismatch = MakeGeometryAsset();
    cwMismatch.regions[0].trimSize = assets::SpritePixelSize{2U, 3U};
    cwMismatch.regions[0].sourceSize = assets::SpritePixelSize{4U, 4U};
    cwMismatch.regions[0].packedRect = assets::SpritePixelRect{12U, 18U, 2U, 3U};
    cwMismatch.regions[0].packedRotation = assets::SpritePackedRotation::Cw90;
    const ResolvedSpriteRegion cwSelection = ResolveOnlyRegion(cwMismatch);
    EXPECT_EQ(
        BuildSpriteDrawQuad(cwSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::PackedExtentMismatch, SpriteGeometryField::PackedRect}));

    assets::SpriteAsset unsupported = MakeGeometryAsset();
    const ResolvedSpriteRegion unsupportedSelection = ResolveOnlyRegion(unsupported);
    unsupported.regions[0].packedRotation = static_cast<assets::SpritePackedRotation>(255U);
    EXPECT_EQ(
        BuildSpriteDrawQuad(unsupportedSelection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{
            SpriteGeometryError::UnsupportedPackedRotation,
            SpriteGeometryField::PackedRotation}));
}

TEST(SpriteGeometry2DTests, DrawRejectsInvalidPosePixelsPerUnitPivotAndOverflowWithoutPartialOutput)
{
    assets::SpriteAsset asset = MakeGeometryAsset();
    ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    SpriteDrawQuad draw{};
    draw.topLeft.position = Float2{123.0F, 456.0F};

    scene::SpritePose2D invalidPose{};
    invalidPose.transform.position.x = std::numeric_limits<float>::infinity();
    EXPECT_EQ(
        BuildSpriteDrawQuad(selection, invalidPose, 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPose, SpriteGeometryField::Pose}));
    EXPECT_EQ(draw, SpriteDrawQuad{});

    EXPECT_EQ(
        BuildSpriteDrawQuad(selection, DefaultPose(), 0.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPixelsPerUnit, SpriteGeometryField::PixelsPerUnit}));
    EXPECT_EQ(draw, SpriteDrawQuad{});

    asset.regions[0].pivot.denominator = 0;
    EXPECT_EQ(
        BuildSpriteDrawQuad(selection, DefaultPose(), 1.0F, draw),
        (SpriteGeometryStatus{SpriteGeometryError::InvalidPivot, SpriteGeometryField::Pivot}));
    EXPECT_EQ(draw, SpriteDrawQuad{});

    asset = MakeGeometryAsset();
    asset.regions[0].pivot = assets::SpriteRationalPivot{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::min(),
        1};
    selection = ResolveOnlyRegion(asset);
    EXPECT_EQ(
        BuildSpriteDrawQuad(
            selection,
            DefaultPose(),
            std::numeric_limits<float>::denorm_min(),
            draw),
        (SpriteGeometryStatus{SpriteGeometryError::GeometryOverflow, SpriteGeometryField::DrawQuad}));
    EXPECT_EQ(draw, SpriteDrawQuad{});
}

TEST(SpriteGeometry2DTests, ReusesCallerOwnedDrawOutputAcrossSteadyStateExtraction)
{
    const assets::SpriteAsset asset = MakeGeometryAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const scene::SpritePose2D pose = DefaultPose();
    SpriteDrawQuad draw{};

    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration)
    {
        ASSERT_TRUE(BuildSpriteDrawQuad(selection, pose, 1.0F, draw).Succeeded());
    }
    ExpectNear(draw.topLeft.position, 0.0F, 1.0F);
    ExpectNear(draw.topLeft.uv, 12.0F / 64.0F, 18.0F / 64.0F);
}
} // namespace
} // namespace trace2d::render
