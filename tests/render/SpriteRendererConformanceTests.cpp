#include <trace2d/render/SpriteBatch2D.hpp>
#include <trace2d/render/SpriteGeometry2D.hpp>
#include <trace2d/render/SpritePixelPerfect2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
[[nodiscard]] trace2d::assets::SpriteAsset MakeSr8Asset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr8-conformance.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr8-conformance.png",
            SpritePixelSize{1U, 2U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "frame",
            "page",
            SpritePixelSize{4U, 3U},
            SpritePixelOffset{1U, 1U},
            SpritePixelSize{2U, 1U},
            SpritePixelRect{0U, 0U, 1U, 2U},
            SpriteRationalPivot{4, 3, 2},
            SpritePackedRotation::Cw90,
        },
    };
    return asset;
}

[[nodiscard]] trace2d::render::ResolvedSpriteRegion ResolveOnlyRegion(
    const trace2d::assets::SpriteAsset& asset)
{
    trace2d::render::ResolvedSpriteRegion selection{};
    EXPECT_TRUE(trace2d::render::ResolveSpriteRegionByIndices(
        &asset, 0U, 0U, selection).Succeeded());
    return selection;
}

void ExpectNear(
    const trace2d::render::Float2 actual,
    const float x,
    const float y,
    const float epsilon = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, x, epsilon);
    EXPECT_NEAR(actual.y, y, epsilon);
}
} // namespace

TEST(SpriteRendererConformanceTests, Sr8CanonicalTrimPivotCw90AndPresentationTimeCompose)
{
    using namespace trace2d;

    const assets::SpriteAsset asset = MakeSr8Asset();
    const render::ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);

    scene::SpritePose2D pose{};
    render::SpriteDrawQuad draw{};
    ASSERT_TRUE(render::BuildSpriteDrawQuad(selection, pose, 1.0F, draw).Succeeded());

    // The canonical source is 4x3, while only source [1,1 -> 3,2] is stored. The exact
    // rational pivot (2, 1.5) keeps the visible trimmed quad centered without allowing
    // packed storage orientation to change logical placement.
    ExpectNear(draw.topLeft.position, -1.0F, 0.5F);
    ExpectNear(draw.topRight.position, 1.0F, 0.5F);
    ExpectNear(draw.bottomRight.position, 1.0F, -0.5F);
    ExpectNear(draw.bottomLeft.position, -1.0F, -0.5F);

    // A cw90-packed 2x1 logical region occupies a 1x2 packed rectangle. SR2 restores
    // logical orientation by UV permutation only.
    ExpectNear(draw.topLeft.uv, 1.0F, 0.0F);
    ExpectNear(draw.topRight.uv, 1.0F, 1.0F);
    ExpectNear(draw.bottomRight.uv, 0.0F, 1.0F);
    ExpectNear(draw.bottomLeft.uv, 0.0F, 0.0F);

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 9.0F};
    render::SpritePixelPerfectViewport2D viewport{};
    ASSERT_TRUE(render::BuildSpritePixelPerfectViewport(
        camera, 16U, 9U, 160U, 90U, viewport).Succeeded());

    scene::SpritePose2D previous{};
    scene::SpritePose2D current{};
    current.transform.position.x = 0.25F;
    const scene::SpritePoseHistory2D history{previous, current};

    render::SpritePixelPerfectMapping2D exact{};
    ASSERT_TRUE(render::ResolveSpritePixelPerfectPose(
        selection,
        history,
        1.0F,
        viewport,
        render::SpritePixelPerfectPoseRequest{
            render::SpritePresentationTimeMode::AuthoritativeCurrent,
            0.0F},
        exact).Succeeded());
    EXPECT_EQ(exact.timeMode, render::SpritePresentationTimeMode::AuthoritativeCurrent);
    EXPECT_FLOAT_EQ(exact.interpolationAlpha, 1.0F);

    render::SpritePixelPerfectMapping2D interpolated{};
    ASSERT_TRUE(render::ResolveSpritePixelPerfectPose(
        selection,
        history,
        1.0F,
        viewport,
        render::SpritePixelPerfectPoseRequest{
            render::SpritePresentationTimeMode::Interpolated,
            0.5F},
        interpolated).Succeeded());
    EXPECT_EQ(interpolated.timeMode, render::SpritePresentationTimeMode::Interpolated);
    EXPECT_FLOAT_EQ(interpolated.interpolationAlpha, 0.5F);

    // Presentation resolution is derived work and may not rewrite authoritative history.
    EXPECT_EQ(history.previous, previous);
    EXPECT_EQ(history.current, current);
}

TEST(SpriteRendererConformanceTests, Sr8CommittedStructuralWorkloadHasExactRawMetrics)
{
    using namespace trace2d::render;

    constexpr std::size_t SpriteCount = 1024U;
    std::array<SpriteBatchItem2D, SpriteCount> items{};

    for (std::size_t index = 0U; index < items.size(); ++index)
    {
        SpriteBatchItem2D& item = items[index];
        item.compatibility.texture = 1U;
        item.visible = (index % 4U) != 0U;
        item.quadCount = (index % 16U) == 1U ? 4U : 1U;
    }

    // Three isolated compatibility changes produce six boundaries while preserving the
    // caller/painter sequence. Culled gaps are intentionally interleaved and do not split runs.
    items[257U].compatibility.texture = 2U;
    items[513U].compatibility.sampler = SpriteSamplerCompatibility::Linear;
    items[769U].compatibility.blend = SpriteBlendCompatibility::Additive;

    const SpritePresentationBatchMeasurement2D measurement =
        MeasureContiguousSpritePresentationBatches(items);

    EXPECT_EQ(measurement.submittedSprites, 1024U);
    EXPECT_EQ(measurement.visibleSprites, 768U);
    EXPECT_EQ(measurement.culledSprites, 256U);
    EXPECT_EQ(measurement.visibleQuads, 960U);
    EXPECT_EQ(measurement.contiguousRuns, 7U);
}
