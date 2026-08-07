#include <trace2d/render/RenderData.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>

namespace
{
TEST(RenderDataTests, BuildsOrthographicViewFromTargetAspectRatio)
{
    trace2d::render::OrthographicCamera camera{};
    camera.center = trace2d::render::Float2{2.0F, -3.0F};
    camera.verticalSize = 8.0F;

    trace2d::render::OrthographicView view{};
    ASSERT_TRUE(trace2d::render::TryBuildOrthographicView(camera, 1600, 800, view));

    EXPECT_EQ(view.center, camera.center);
    EXPECT_FLOAT_EQ(view.halfExtents.x, 8.0F);
    EXPECT_FLOAT_EQ(view.halfExtents.y, 4.0F);
    EXPECT_FLOAT_EQ(view.clipScale.x, 0.125F);
    EXPECT_FLOAT_EQ(view.clipScale.y, 0.25F);
}

TEST(RenderDataTests, WorldToClipUsesEngineYUpNdcConvention)
{
    trace2d::render::OrthographicCamera camera{};
    camera.center = trace2d::render::Float2{2.0F, -3.0F};
    camera.verticalSize = 8.0F;

    trace2d::render::OrthographicView view{};
    ASSERT_TRUE(trace2d::render::TryBuildOrthographicView(camera, 1600, 800, view));

    EXPECT_EQ(
        trace2d::render::WorldToClip(view, trace2d::render::Float2{2.0F, -3.0F}),
        (trace2d::render::Float2{0.0F, 0.0F}));
    EXPECT_EQ(
        trace2d::render::WorldToClip(view, trace2d::render::Float2{10.0F, 1.0F}),
        (trace2d::render::Float2{1.0F, 1.0F}));
    EXPECT_EQ(
        trace2d::render::WorldToClip(view, trace2d::render::Float2{-6.0F, -7.0F}),
        (trace2d::render::Float2{-1.0F, -1.0F}));
}

TEST(RenderDataTests, RejectsInvalidCameraOrTargetAndClearsOutput)
{
    trace2d::render::OrthographicView view{};
    view.center = trace2d::render::Float2{100.0F, 100.0F};
    view.halfExtents = trace2d::render::Float2{100.0F, 100.0F};
    view.clipScale = trace2d::render::Float2{100.0F, 100.0F};

    trace2d::render::OrthographicCamera camera{};
    EXPECT_FALSE(trace2d::render::TryBuildOrthographicView(camera, 0, 720, view));
    EXPECT_EQ(view, trace2d::render::OrthographicView{});

    camera.verticalSize = 0.0F;
    EXPECT_FALSE(trace2d::render::TryBuildOrthographicView(camera, 1280, 720, view));
    EXPECT_EQ(view, trace2d::render::OrthographicView{});

    camera.verticalSize = 10.0F;
    camera.center.x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(trace2d::render::TryBuildOrthographicView(camera, 1280, 720, view));
    EXPECT_EQ(view, trace2d::render::OrthographicView{});
}

TEST(RenderDataTests, SpriteVisibilityUsesInclusiveAxisAlignedBounds)
{
    trace2d::render::OrthographicCamera camera{};
    camera.verticalSize = 6.0F;

    trace2d::render::OrthographicView view{};
    ASSERT_TRUE(trace2d::render::TryBuildOrthographicView(camera, 1000, 600, view));
    ASSERT_FLOAT_EQ(view.halfExtents.x, 5.0F);
    ASSERT_FLOAT_EQ(view.halfExtents.y, 3.0F);

    trace2d::render::SpriteRenderData visible{};
    visible.center = trace2d::render::Float2{0.0F, 0.0F};
    visible.halfExtents = trace2d::render::Float2{0.5F, 0.5F};
    EXPECT_TRUE(trace2d::render::IsSpriteVisible(view, visible));

    trace2d::render::SpriteRenderData touchingRightEdge = visible;
    touchingRightEdge.center.x = 5.5F;
    EXPECT_TRUE(trace2d::render::IsSpriteVisible(view, touchingRightEdge));

    trace2d::render::SpriteRenderData outsideRightEdge = visible;
    outsideRightEdge.center.x = 5.5001F;
    EXPECT_FALSE(trace2d::render::IsSpriteVisible(view, outsideRightEdge));

    trace2d::render::SpriteRenderData outsideTopEdge = visible;
    outsideTopEdge.center.y = 3.5001F;
    EXPECT_FALSE(trace2d::render::IsSpriteVisible(view, outsideTopEdge));
}

TEST(RenderDataTests, SpriteDrawOrderUsesLayerThenStableOrder)
{
    std::array<trace2d::render::SpriteRenderData, 4> sprites{};
    sprites[0].layer = 2;
    sprites[0].stableOrder = 5;
    sprites[1].layer = 1;
    sprites[1].stableOrder = 9;
    sprites[2].layer = 1;
    sprites[2].stableOrder = 3;
    sprites[3].layer = 0;
    sprites[3].stableOrder = 100;

    std::sort(sprites.begin(), sprites.end(), trace2d::render::SpriteDrawOrderLess{});

    EXPECT_EQ(sprites[0].layer, 0);
    EXPECT_EQ(sprites[0].stableOrder, 100U);
    EXPECT_EQ(sprites[1].layer, 1);
    EXPECT_EQ(sprites[1].stableOrder, 3U);
    EXPECT_EQ(sprites[2].layer, 1);
    EXPECT_EQ(sprites[2].stableOrder, 9U);
    EXPECT_EQ(sprites[3].layer, 2);
    EXPECT_EQ(sprites[3].stableOrder, 5U);
}

TEST(RenderDataTests, EqualSpriteDrawKeysAreEquivalentForOrdering)
{
    trace2d::render::SpriteRenderData left{};
    left.layer = 3;
    left.stableOrder = 77;

    trace2d::render::SpriteRenderData right = left;
    right.center = trace2d::render::Float2{10.0F, 20.0F};

    const trace2d::render::SpriteDrawOrderLess less{};
    EXPECT_FALSE(less(left, right));
    EXPECT_FALSE(less(right, left));
}
} // namespace
