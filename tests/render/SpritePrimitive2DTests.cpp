#include <trace2d/render/SpritePrimitive2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <vector>

namespace trace2d::render
{
namespace
{
assets::SpriteAsset MakePrimitiveAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/primitive.sprite.toml";
    asset.pages = {
        assets::SpriteAtlasPage{
            "main",
            "textures/primitive.png",
            assets::SpritePixelSize{64U, 64U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "panel",
            "main",
            assets::SpritePixelSize{6U, 6U},
            assets::SpritePixelOffset{0U, 0U},
            assets::SpritePixelSize{6U, 6U},
            assets::SpritePixelRect{10U, 20U, 6U, 6U},
            assets::SpriteRationalPivot{3, 3, 1},
            assets::SpritePackedRotation::None,
            assets::SpritePixelBorder{2U, 2U, 2U, 2U},
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

void ExpectNear(const Float2 actual, const float x, const float y, const float epsilon = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, x, epsilon);
    EXPECT_NEAR(actual.y, y, epsilon);
}

TEST(SpritePrimitive2DTests, QuadDelegatesToExistingSr2Geometry)
{
    const assets::SpriteAsset asset = MakePrimitiveAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const scene::SpritePose2D pose{};

    SpriteDrawQuad legacy{};
    ASSERT_TRUE(BuildSpriteDrawQuad(selection, pose, 2.0F, legacy).Succeeded());

    std::array<SpritePrimitivePatch2D, 1U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        pose,
        2.0F,
        SpritePrimitive2D{SpritePrimitiveMode::Quad, Float2{}},
        patches,
        patchCount).Succeeded());

    ASSERT_EQ(patchCount, 1U);
    EXPECT_EQ(patches[0].quad, legacy);
    ExpectNear(
        patches[0].sampleBounds.minimum,
        10.5F / 64.0F,
        20.5F / 64.0F);
    ExpectNear(
        patches[0].sampleBounds.maximum,
        15.5F / 64.0F,
        25.5F / 64.0F);
}

TEST(SpritePrimitive2DTests, SlicedBuildsNineCellsAndPreservesNormalizedPivot)
{
    const assets::SpriteAsset asset = MakePrimitiveAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePrimitive2D primitive{
        SpritePrimitiveMode::Sliced,
        Float2{10.0F, 8.0F},
    };

    std::array<SpritePrimitivePatch2D, 9U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        patches,
        patchCount).Succeeded());

    ASSERT_EQ(patchCount, 9U);

    // Target pivot remains centered: target 10x8 spans x [-5,+5], y [+4,-4].
    ExpectNear(patches[0].quad.topLeft.position, -5.0F, 4.0F);
    ExpectNear(patches[0].quad.bottomRight.position, -3.0F, 2.0F);

    // Center cell is source [2,2]-[4,4] stretched to target [2,2]-[8,6].
    ExpectNear(patches[4].quad.topLeft.position, -3.0F, 2.0F);
    ExpectNear(patches[4].quad.bottomRight.position, 3.0F, -2.0F);
    ExpectNear(patches[4].quad.topLeft.uv, 12.0F / 64.0F, 22.0F / 64.0F);
    ExpectNear(patches[4].quad.bottomRight.uv, 14.0F / 64.0F, 24.0F / 64.0F);
    ExpectNear(patches[4].sampleBounds.minimum, 12.5F / 64.0F, 22.5F / 64.0F);
    ExpectNear(patches[4].sampleBounds.maximum, 13.5F / 64.0F, 23.5F / 64.0F);
}

TEST(SpritePrimitive2DTests, UndersizedTargetCompressesOpposingBordersAndDropsZeroCenter)
{
    const assets::SpriteAsset asset = MakePrimitiveAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePrimitive2D primitive{
        SpritePrimitiveMode::Sliced,
        Float2{2.0F, 2.0F},
    };

    std::array<SpritePrimitivePatch2D, 9U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        patches,
        patchCount).Succeeded());

    ASSERT_EQ(patchCount, 4U);
    ExpectNear(patches[0].quad.topLeft.position, -1.0F, 1.0F);
    ExpectNear(patches[0].quad.bottomRight.position, 0.0F, 0.0F);
    ExpectNear(patches[3].quad.topLeft.position, 0.0F, 0.0F);
    ExpectNear(patches[3].quad.bottomRight.position, 1.0F, -1.0F);
}

TEST(SpritePrimitive2DTests, TrimIntersectionKeepsTransparentLogicalGaps)
{
    assets::SpriteAsset asset = MakePrimitiveAsset();
    asset.regions[0].trimOffset = assets::SpritePixelOffset{1U, 1U};
    asset.regions[0].trimSize = assets::SpritePixelSize{4U, 4U};
    asset.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 4U, 4U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);

    std::array<SpritePrimitivePatch2D, 9U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        SpritePrimitive2D{SpritePrimitiveMode::Sliced, Float2{10.0F, 10.0F}},
        patches,
        patchCount).Succeeded());

    ASSERT_EQ(patchCount, 9U);
    // Source x [0,2] is visible only at [1,2], so target x [0,2] begins drawing at x=1.
    // With a centered 10px target this leaves the transparent logical gap [-5,-4].
    ExpectNear(patches[0].quad.topLeft.position, -4.0F, 4.0F);
    ExpectNear(patches[0].quad.bottomRight.position, -3.0F, 3.0F);
}

TEST(SpritePrimitive2DTests, Cw90MapsArbitrarySubrectWithoutChangingLogicalPlacement)
{
    assets::SpriteAsset unrotated = MakePrimitiveAsset();
    unrotated.regions[0].trimOffset = assets::SpritePixelOffset{1U, 1U};
    unrotated.regions[0].trimSize = assets::SpritePixelSize{4U, 4U};
    unrotated.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 4U, 4U};

    assets::SpriteAsset rotated = unrotated;
    rotated.regions[0].packedRotation = assets::SpritePackedRotation::Cw90;
    rotated.regions[0].packedRect = assets::SpritePixelRect{30U, 40U, 4U, 4U};

    const ResolvedSpriteRegion unrotatedSelection = ResolveOnlyRegion(unrotated);
    const ResolvedSpriteRegion rotatedSelection = ResolveOnlyRegion(rotated);
    const SpritePrimitive2D primitive{SpritePrimitiveMode::Sliced, Float2{10.0F, 10.0F}};

    std::array<SpritePrimitivePatch2D, 9U> unrotatedPatches{};
    std::array<SpritePrimitivePatch2D, 9U> rotatedPatches{};
    std::size_t unrotatedCount = 0U;
    std::size_t rotatedCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        unrotatedSelection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        unrotatedPatches,
        unrotatedCount).Succeeded());
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        rotatedSelection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        rotatedPatches,
        rotatedCount).Succeeded());

    ASSERT_EQ(unrotatedCount, rotatedCount);
    ASSERT_EQ(rotatedCount, 9U);
    EXPECT_EQ(rotatedPatches[0].quad.topLeft.position, unrotatedPatches[0].quad.topLeft.position);
    EXPECT_EQ(rotatedPatches[0].quad.bottomRight.position, unrotatedPatches[0].quad.bottomRight.position);

    // First visible patch samples source [1,1]-[2,2]. local TL=(0,0) maps to packed TR.
    ExpectNear(rotatedPatches[0].quad.topLeft.uv, 34.0F / 64.0F, 40.0F / 64.0F);
    ExpectNear(rotatedPatches[0].quad.topRight.uv, 34.0F / 64.0F, 41.0F / 64.0F);
    ExpectNear(rotatedPatches[0].quad.bottomRight.uv, 33.0F / 64.0F, 41.0F / 64.0F);
    ExpectNear(rotatedPatches[0].quad.bottomLeft.uv, 33.0F / 64.0F, 40.0F / 64.0F);
}

TEST(SpritePrimitive2DTests, TiledRepeatsCenterAndClipsFinalPartialTile)
{
    assets::SpriteAsset asset = MakePrimitiveAsset();
    asset.regions[0].border = assets::SpritePixelBorder{1U, 1U, 1U, 1U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePrimitive2D primitive{SpritePrimitiveMode::Tiled, Float2{11.0F, 10.0F}};

    std::size_t required = 0U;
    ASSERT_TRUE(CountSpritePrimitivePatches(selection, primitive, required).Succeeded());
    ASSERT_EQ(required, 20U);

    std::array<SpritePrimitivePatch2D, 20U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        patches,
        patchCount).Succeeded());
    ASSERT_EQ(patchCount, 20U);

    // Row 1 / center cell starts at index 7. Its third X tile is partial: target x [9,10]
    // inside the target's [1,10] center span and samples only source x [1,2].
    const SpritePrimitivePatch2D& partialCenter = patches[9];
    ExpectNear(partialCenter.quad.topLeft.position, 3.5F, 4.0F);
    ExpectNear(partialCenter.quad.topRight.position, 4.5F, 4.0F);
    ExpectNear(partialCenter.quad.topLeft.uv, 11.0F / 64.0F, 21.0F / 64.0F);
    ExpectNear(partialCenter.quad.topRight.uv, 12.0F / 64.0F, 21.0F / 64.0F);

    // A one-pixel partial tile collapses the linear clamp to its texel center.
    EXPECT_NEAR(partialCenter.sampleBounds.minimum.x, 11.5F / 64.0F, 1.0e-5F);
    EXPECT_NEAR(partialCenter.sampleBounds.maximum.x, 11.5F / 64.0F, 1.0e-5F);
}

TEST(SpritePrimitive2DTests, SubTexelPartialTileClampsToRepresentedTexelCenter)
{
    assets::SpriteAsset asset = MakePrimitiveAsset();
    asset.regions[0].border = assets::SpritePixelBorder{1U, 1U, 1U, 1U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePrimitive2D primitive{SpritePrimitiveMode::Tiled, Float2{6.5F, 6.0F}};

    std::array<SpritePrimitivePatch2D, 12U> patches{};
    std::size_t patchCount = 0U;
    ASSERT_TRUE(BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        patches,
        patchCount).Succeeded());
    ASSERT_EQ(patchCount, 12U);

    // Row 1 / center cell emits one full four-pixel tile followed by a half-pixel tile.
    // Exact UV geometry remains [11, 11.5] atlas pixels, but linear sampling must collapse to
    // the represented texel center 11.5 rather than the geometric midpoint 11.25.
    const SpritePrimitivePatch2D& halfTexelCenter = patches[6];
    ExpectNear(halfTexelCenter.quad.topLeft.uv, 11.0F / 64.0F, 21.0F / 64.0F);
    ExpectNear(halfTexelCenter.quad.topRight.uv, 11.5F / 64.0F, 21.0F / 64.0F);
    EXPECT_NEAR(halfTexelCenter.sampleBounds.minimum.x, 11.5F / 64.0F, 1.0e-5F);
    EXPECT_NEAR(halfTexelCenter.sampleBounds.maximum.x, 11.5F / 64.0F, 1.0e-5F);
}

TEST(SpritePrimitive2DTests, ReportsRequiredCapacityWithoutPartialWrites)
{
    const assets::SpriteAsset asset = MakePrimitiveAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePrimitive2D primitive{SpritePrimitiveMode::Sliced, Float2{10.0F, 10.0F}};

    std::array<SpritePrimitivePatch2D, 8U> patches{};
    patches[0].quad.topLeft.position = Float2{123.0F, 456.0F};
    const SpritePrimitivePatch2D sentinel = patches[0];
    std::size_t patchCount = 0U;
    const SpritePrimitiveStatus status = BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        1.0F,
        primitive,
        patches,
        patchCount);

    EXPECT_EQ(
        status,
        (SpritePrimitiveStatus{
            SpritePrimitiveError::InsufficientCapacity,
            SpritePrimitiveField::OutputCapacity,
            SpriteGeometryStatus{},
        }));
    EXPECT_EQ(patchCount, 9U);
    EXPECT_EQ(patches[0], sentinel);
}

TEST(SpritePrimitive2DTests, AcceptsExactExpansionLimitAndRejectsOneOver)
{
    assets::SpriteAsset asset = MakePrimitiveAsset();
    asset.regions[0].sourceSize = assets::SpritePixelSize{1U, 1U};
    asset.regions[0].trimSize = assets::SpritePixelSize{1U, 1U};
    asset.regions[0].packedRect = assets::SpritePixelRect{10U, 20U, 1U, 1U};
    asset.regions[0].pivot = assets::SpriteRationalPivot{0, 0, 1};
    asset.regions[0].border = assets::SpritePixelBorder{};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);

    std::size_t patchCount = 0U;
    EXPECT_TRUE(CountSpritePrimitivePatches(
        selection,
        SpritePrimitive2D{
            SpritePrimitiveMode::Tiled,
            Float2{static_cast<float>(MaximumSpritePrimitiveQuads), 1.0F},
        },
        patchCount).Succeeded());
    EXPECT_EQ(patchCount, MaximumSpritePrimitiveQuads);

    const SpritePrimitiveStatus over = CountSpritePrimitivePatches(
        selection,
        SpritePrimitive2D{
            SpritePrimitiveMode::Tiled,
            Float2{static_cast<float>(MaximumSpritePrimitiveQuads + 1U), 1.0F},
        },
        patchCount);
    EXPECT_EQ(over.error, SpritePrimitiveError::ExpansionLimit);
    EXPECT_EQ(over.field, SpritePrimitiveField::PatchCount);
    EXPECT_EQ(patchCount, MaximumSpritePrimitiveQuads + 1U);
}

TEST(SpritePrimitive2DTests, RejectsInvalidBorderTargetAndMode)
{
    assets::SpriteAsset asset = MakePrimitiveAsset();
    asset.regions[0].border = assets::SpritePixelBorder{4U, 0U, 3U, 0U};
    ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    std::size_t patchCount = 0U;
    EXPECT_EQ(
        CountSpritePrimitivePatches(
            selection,
            SpritePrimitive2D{SpritePrimitiveMode::Sliced, Float2{10.0F, 10.0F}},
            patchCount).error,
        SpritePrimitiveError::InvalidBorder);

    asset = MakePrimitiveAsset();
    selection = ResolveOnlyRegion(asset);
    EXPECT_EQ(
        CountSpritePrimitivePatches(
            selection,
            SpritePrimitive2D{SpritePrimitiveMode::Sliced, Float2{0.0F, 10.0F}},
            patchCount).error,
        SpritePrimitiveError::InvalidTargetSize);
    EXPECT_EQ(
        CountSpritePrimitivePatches(
            selection,
            SpritePrimitive2D{
                SpritePrimitiveMode::Tiled,
                Float2{std::numeric_limits<float>::infinity(), 10.0F},
            },
            patchCount).error,
        SpritePrimitiveError::InvalidTargetSize);
    EXPECT_EQ(
        CountSpritePrimitivePatches(
            selection,
            SpritePrimitive2D{static_cast<SpritePrimitiveMode>(255U), Float2{10.0F, 10.0F}},
            patchCount).error,
        SpritePrimitiveError::UnsupportedMode);
}
} // namespace
} // namespace trace2d::render
