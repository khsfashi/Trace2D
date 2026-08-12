#include <trace2d/render/SpriteBatch2D.hpp>

#include <gtest/gtest.h>

#include <array>

namespace
{
[[nodiscard]] trace2d::render::SpriteDrawQuad MakeQuad(
    const float minimumX,
    const float minimumY,
    const float maximumX,
    const float maximumY)
{
    using namespace trace2d::render;

    SpriteDrawQuad quad{};
    quad.topLeft.position = Float2{minimumX, maximumY};
    quad.topRight.position = Float2{maximumX, maximumY};
    quad.bottomRight.position = Float2{maximumX, minimumY};
    quad.bottomLeft.position = Float2{minimumX, minimumY};
    return quad;
}

[[nodiscard]] trace2d::render::SpriteBatchItem2D Item(
    const trace2d::render::TextureHandle texture,
    const bool visible = true,
    const std::uint32_t quadCount = 1U)
{
    trace2d::render::SpriteBatchItem2D item{};
    item.compatibility.texture = texture;
    item.visible = visible;
    item.quadCount = quadCount;
    return item;
}
} // namespace

TEST(SpriteBatch2DTests, CompatibleVisibleItemsMergeAcrossCulledGap)
{
    using namespace trace2d::render;

    const std::array<SpriteBatchItem2D, 4U> items{
        Item(1U),
        Item(1U, false),
        Item(1U, true, 3U),
        Item(1U, true, 2U),
    };

    const SpritePresentationBatchMeasurement2D measurement =
        MeasureContiguousSpritePresentationBatches(items);

    EXPECT_EQ(measurement.submittedSprites, 4U);
    EXPECT_EQ(measurement.visibleSprites, 3U);
    EXPECT_EQ(measurement.culledSprites, 1U);
    EXPECT_EQ(measurement.visibleQuads, 6U);
    EXPECT_EQ(measurement.contiguousRuns, 1U);
}

TEST(SpriteBatch2DTests, EveryGpuCompatibilityStateBreaksAContiguousRun)
{
    using namespace trace2d::render;

    SpriteBatchItem2D base = Item(1U);
    SpriteBatchItem2D texture = base;
    texture.compatibility.texture = 2U;
    SpriteBatchItem2D material = base;
    material.compatibility.materialPipeline = BuiltInSpriteMaterialPipelineIdentity + 1U;
    SpriteBatchItem2D sampler = base;
    sampler.compatibility.sampler = SpriteSamplerCompatibility::Linear;
    SpriteBatchItem2D blend = base;
    blend.compatibility.blend = SpriteBlendCompatibility::Additive;
    SpriteBatchItem2D mask = base;
    mask.compatibility.mask = SpriteMask2D{SpriteMaskMode::TestInside, 7U};

    const std::array<SpriteBatchItem2D, 6U> items{
        base,
        texture,
        material,
        sampler,
        blend,
        mask,
    };

    EXPECT_EQ(
        MeasureContiguousSpritePresentationBatches(items).contiguousRuns,
        items.size());
}

TEST(SpriteBatch2DTests, ZeroQuadItemEmitsNothingAndDoesNotSplitRun)
{
    using namespace trace2d::render;

    const std::array<SpriteBatchItem2D, 3U> items{
        Item(9U),
        Item(77U, true, 0U),
        Item(9U),
    };

    const SpritePresentationBatchMeasurement2D measurement =
        MeasureContiguousSpritePresentationBatches(items);
    EXPECT_EQ(measurement.visibleSprites, 2U);
    EXPECT_EQ(measurement.culledSprites, 1U);
    EXPECT_EQ(measurement.visibleQuads, 2U);
    EXPECT_EQ(measurement.contiguousRuns, 1U);
}

TEST(SpriteBatch2DTests, VisibilityUsesInclusiveResolvedViewBounds)
{
    using namespace trace2d::render;

    const OrthographicView view{
        Float2{0.0F, 0.0F},
        Float2{1.0F, 1.0F},
        Float2{1.0F, 1.0F},
    };

    EXPECT_TRUE(IsSpritePresentationQuadVisible(
        view,
        MakeQuad(0.75F, -0.25F, 1.0F, 0.25F)));
    EXPECT_TRUE(IsSpritePresentationQuadVisible(
        view,
        MakeQuad(1.0F, -0.25F, 1.5F, 0.25F)));
    EXPECT_FALSE(IsSpritePresentationQuadVisible(
        view,
        MakeQuad(1.01F, -0.25F, 1.5F, 0.25F)));
}

TEST(SpriteBatch2DTests, PrimitiveVisibilityRequiresAnyIntersectingPatch)
{
    using namespace trace2d::render;

    const OrthographicView view{
        Float2{0.0F, 0.0F},
        Float2{1.0F, 1.0F},
        Float2{1.0F, 1.0F},
    };
    std::array<SpritePrimitivePatch2D, 2U> patches{};
    patches[0].quad = MakeQuad(2.0F, 2.0F, 3.0F, 3.0F);
    patches[1].quad = MakeQuad(-0.25F, -0.25F, 0.25F, 0.25F);

    EXPECT_TRUE(IsSpritePresentationPrimitiveVisible(view, patches));
    EXPECT_FALSE(IsSpritePresentationPrimitiveVisible(view, patches.first(1U)));
    EXPECT_FALSE(IsSpritePresentationPrimitiveVisible(view, {}));
}
